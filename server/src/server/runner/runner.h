#pragma once

#include <chrono>
#include <concepts>
#include <cstdint>
#include <curl/curl.h>
#include <exception>
#include <filesystem>
#include <optional>
#include <random>
#include <string>
#include <thread>
#include <zlib.h>

#include "bosim/base/log.h"
#include "bosim/exception.h"
#include "bosim/simulate/settings.h"
#include "bosim/version.h"

#include "server/base/string.h"
#include "server/clients/scheduler.h"
#include "server/common/backup.h"
#include "server/common/error_code.h"
#include "server/common/message_manager.h"
#include "server/common/stats.h"
#include "server/common/utility.h"
#include "server/runner/common.h"
#include "server/runner/simulate.h"
#include "server/runner/upload.h"
#include "server/runner/validate.h"
#include "server/settings/token.h"

#include "mqc3_cloud/common/v1/error_detail.pb.h"
#include "mqc3_cloud/program/v1/quantum_program.pb.h"
#include "mqc3_cloud/scheduler/v1/execution.pb.h"
#include "mqc3_cloud/scheduler/v1/job.pb.h"

struct JobRunnerSettings {
    std::string backend;

    // Validation settings.
    ValidateSettings validation;

    // Upload settings.
    std::filesystem::path backup_root_path;
    std::uint32_t max_upload_retries;
    std::chrono::duration<float> upload_retry_interval_seconds;

    // Report settings.
    std::uint32_t max_report_retries;
    std::chrono::duration<float> report_retry_interval_seconds;
};

template <std::floating_point Float>
struct SimulateSettings {
    std::uint64_t seed;
    std::uint32_t n_shots;
    Float resource_squeezing_level;
    bosim::StateSaveMethod state_save_method;
    Duration timeout;
};

struct SimulateResult {
    mqc3_cloud::program::v1::QuantumProgramResult result;
    Timestamp started_at;
    Timestamp finished_at;
};

struct UploadSettings {
    std::filesystem::path backup_root_path;
    std::uint32_t max_upload_retries{3};
    std::chrono::duration<float> upload_retry_interval_seconds{1.0f};
};

struct UploadResult {
    mqc3_cloud::scheduler::v1::JobUploadedResult uploaded_result;
    Timestamp started_at;
    Timestamp finished_at;
};

template <std::floating_point Float>
class JobRunner {
public:
    explicit JobRunner(const JobRunnerSettings& settings, ExecutionClient* execution_client)
        : settings_{settings}, execution_client_{execution_client},
          critical_error_{MessageManager::GetMessage(CRITICAL_ERROR).ToError()},
          engine_{std::random_device{}()} {}

    void Run(const mqc3_cloud::scheduler::v1::AssignNextJobResponse& response) {
        const auto& job_id = response.job_id();
        auto report = mqc3_cloud::scheduler::v1::ReportExecutionResultRequest();
        report.set_token(GetToken());
        try {
            RunImpl(&report, response);
            Report(job_id, report);
        } catch (const std::exception& ex) {
            LOG_ERROR("[id={}] Unexpected error occurred: {}", job_id, ex.what());

            try {
                MakeErrorReport(&report, job_id, critical_error_);
                Report(job_id, report);
            } catch (const std::exception& ex) {
                LOG_FATAL("[id={}] Failed to report internal server error: {}", job_id, ex.what());
            }
        }
    }

    const ReportLog& GetLog() const { return log_; }
    void ClearLog() { log_.Clear(); }

private:
    void BackupJobIfInternalError(const mqc3_cloud::scheduler::v1::AssignNextJobResponse& response,
                                  const mqc3_cloud::common::v1::ErrorDetail& error) {
        if (!(error.code() == "INTERNAL_ERROR" || error.code() == "CRITICAL_ERROR")) { return; }

        const auto& job_id = response.job_id();
        const auto backup_path = settings_.backup_root_path / fmt::format("{}.job.proto", job_id);
        if (!BackupBinary(backup_path, response.SerializeAsString())) {
            LOG_ERROR("[id={}] Failed to write backup job data with internal error: {}", job_id,
                      backup_path.string());
        }
    }

    void MakeErrorReport(mqc3_cloud::scheduler::v1::ReportExecutionResultRequest* o_report,
                         const std::string& job_id,
                         const mqc3_cloud::common::v1::ErrorDetail& error, bool timeout = false) {
        o_report->set_job_id(job_id);
        o_report->set_status(
            timeout ? mqc3_cloud::scheduler::v1::ExecutionStatus::EXECUTION_STATUS_TIMEOUT
                    : mqc3_cloud::scheduler::v1::ExecutionStatus::EXECUTION_STATUS_FAILURE);
        o_report->mutable_error()->CopyFrom(error);
        o_report->set_actual_backend(settings_.backend);
        o_report->mutable_version()->set_simulator(bosim::GetVersion());
    }

    void RunImpl(mqc3_cloud::scheduler::v1::ReportExecutionResultRequest* o_report,
                 const mqc3_cloud::scheduler::v1::AssignNextJobResponse& response) {
        const auto started_at = Timestamp::Now();

        const auto& job = response.job();
        const auto& job_id = response.job_id();
        const auto& upload_target = response.upload_target();

        /////////////////////////
        // Validate
        /////////////////////////

        LOG_DEBUG_ALWAYS("[id={}] Starting validate.", job_id);
        auto validator = Validator(settings_.validation);
        {
            try {
                validator.Initialize(job.program(), job.settings());

                const auto settings_error = validator.ValidateExecutionSettings(job.settings());
                if (settings_error) {
                    BackupJobIfInternalError(response, settings_error.value());
                    MakeErrorReport(o_report, job_id, settings_error.value());
                    return;
                }
                const auto program_error = validator.ValidateProgram(job.program());
                if (program_error) {
                    BackupJobIfInternalError(response, program_error.value());
                    MakeErrorReport(o_report, job_id, program_error.value());
                    return;
                }
            } catch (const std::exception& ex) {
                LOG_ERROR("[id={}] Failed to validate program: {}", job_id, ex.what());
                const auto error =
                    MessageManager::GetMessage(INTERNAL_ERROR, fmt::arg("when", "validating"))
                        .ToError();
                BackupJobIfInternalError(response, error);
                MakeErrorReport(o_report, job_id, error);
                return;
            }
        }
        LOG_DEBUG_ALWAYS("[id={}] Finished validate.", job_id);

        /////////////////////////
        // Simulate.
        /////////////////////////

        LOG_DEBUG_ALWAYS("[id={}] Starting simulate.", job_id);
        auto simulate_result =
            SimulateResult{.result = mqc3_cloud::program::v1::QuantumProgramResult(),
                           .started_at = Timestamp::Now(),
                           .finished_at = Timestamp::Now()};
        {
            const auto settings = SimulateSettings<Float>{
                // TODO: Get seed from job settings if seed field is added.
                .seed = engine_(),
                .n_shots = static_cast<std::uint32_t>(job.settings().n_shots()),
                .resource_squeezing_level = job.settings().resource_squeezing_level(),
                .state_save_method = validator.GetStateSaveMethod(),
                .timeout = Duration(job.settings().timeout())};

            const auto error = Simulate(&simulate_result, job_id, settings, job.program());
            if (error) {
                BackupJobIfInternalError(response, error.value());
                MakeErrorReport(o_report, job_id, error.value());
                return;
            }
        }
        LOG_DEBUG_ALWAYS("[id={}] Finished simulate.", job_id);

        /////////////////////////
        // Upload result.
        /////////////////////////

        LOG_DEBUG_ALWAYS("[id={}] Starting upload result.", job_id);

        auto upload_result =
            UploadResult{.uploaded_result = mqc3_cloud::scheduler::v1::JobUploadedResult(),
                         .started_at = Timestamp::Now(),
                         .finished_at = Timestamp::Now()};
        {
            const auto settings = UploadSettings{
                .backup_root_path = settings_.backup_root_path,
                .max_upload_retries = settings_.max_upload_retries,
                .upload_retry_interval_seconds = settings_.upload_retry_interval_seconds,
            };

            const auto error =
                Upload(&upload_result, job_id, settings, upload_target, simulate_result.result);
            if (error) {
                BackupJobIfInternalError(response, error.value());
                MakeErrorReport(o_report, job_id, error.value());
                return;
            }
        }

        LOG_DEBUG_ALWAYS("[id={}] Finished upload result.", job_id);

        /////////////////////////
        // Construct report.
        /////////////////////////

        LOG_DEBUG_ALWAYS("[id={}] Starting construct report.", job_id);

        o_report->set_job_id(job_id);
        o_report->set_status(mqc3_cloud::scheduler::v1::ExecutionStatus::EXECUTION_STATUS_SUCCESS);
        // skip: error
        o_report->mutable_uploaded_result()->CopyFrom(upload_result.uploaded_result);
        o_report->mutable_timestamps()->mutable_execution_started_at()->CopyFrom(
            simulate_result.started_at.Data());
        o_report->mutable_timestamps()->mutable_execution_finished_at()->CopyFrom(
            simulate_result.finished_at.Data());
        o_report->set_actual_backend(settings_.backend);
        o_report->mutable_version()->set_simulator(bosim::GetVersion());

        LOG_DEBUG_ALWAYS("[id={}] Finished construct report.", job_id);

        const auto finished_at = Timestamp::Now();
        LOG_INFO("[id={}] total={:.2f}[ms] run={:.2f}[ms] upload={:.2f}[ms]", job_id,
                 1000.0 * (finished_at - started_at).Seconds(),
                 1000.0 * (simulate_result.finished_at - simulate_result.started_at).Seconds(),
                 1000.0 * (upload_result.finished_at - upload_result.started_at).Seconds());
    }

    void RefreshUploadUrlIfNeeded(mqc3_cloud::scheduler::v1::JobResultUploadTarget* o_upload_target,
                                  const std::string& job_id, std::uint32_t attempt) {
        if (attempt == 0 && Timestamp::Now() < Timestamp(o_upload_target->expires_at())) {
            // If upload url is not expired, and it's the first attempt,
            // do not refresh upload url.
            return;
        }

        // Refresh upload URL since it's expired or a retry is happening.
        auto request = mqc3_cloud::scheduler::v1::RefreshUploadUrlRequest();
        request.set_token(GetToken());
        request.set_job_id(job_id);

        auto response = execution_client_->RefreshUploadUrl(request);
        o_upload_target->CopyFrom(response.upload_target());
    }

    std::optional<mqc3_cloud::common::v1::ErrorDetail> Simulate(
        SimulateResult* o_result, const std::string& job_id,
        const SimulateSettings<Float>& settings,
        const mqc3_cloud::program::v1::QuantumProgram& program) {
        try {
            o_result->started_at = Timestamp::Now();

            const auto simulate_settings = bosim::SimulateSettings{
                .n_shots = settings.n_shots,
                .backend = GetBackend(settings_.backend),
                .save_state_method = settings.state_save_method,
                .seed = settings.seed,
                .timeout =
                    std::chrono::seconds(static_cast<std::int64_t>(settings.timeout.Seconds())),
            };

            if (program.has_circuit()) {
                LOG_DEBUG_ALWAYS("[id={}] Simulate circuit.", job_id);
                SimulateCircuit<Float>(&o_result->result, simulate_settings, program.circuit());
            } else if (program.has_graph()) {
                LOG_DEBUG_ALWAYS("[id={}] Simulate graph.", job_id);
                SimulateGraph<Float>(&o_result->result, simulate_settings,
                                     settings.resource_squeezing_level, program.graph());
            } else if (program.has_machinery()) {
                LOG_DEBUG_ALWAYS("[id={}] Simulate machinery.", job_id);
                SimulateMachinery<Float>(&o_result->result, simulate_settings,
                                         settings.resource_squeezing_level, program.machinery());
            }

            o_result->finished_at = Timestamp::Now();
            return std::nullopt;
        } catch (const bosim::ParseError& ex) {
            LOG_ERROR("[id={}] [error={}] Failed to parse program: {}", job_id, ex.id, ex.what());
            return MessageManager::GetMessage(INVALID_PROGRAM,
                                              fmt::arg("reason", RemovePeriod(ex.what())))
                .ToError();
        } catch (const bosim::SimulationError& ex) {
            LOG_ERROR("[id={}] [error={}] Failed to simulate program: {}", job_id, ex.id,
                      ex.what());
            return MessageManager::GetMessage(EXECUTION_FAILED,
                                              fmt::arg("reason", RemovePeriod(ex.what())))
                .ToError();
        } catch (const bosim::OutOfRange& ex) {
            LOG_ERROR("[id={}] [error={}] Failed to simulate program: {}", job_id, ex.id,
                      ex.what());
            return MessageManager::GetMessage(EXECUTION_FAILED,
                                              fmt::arg("reason", RemovePeriod(ex.what())))
                .ToError();
        } catch (const bosim::Exception& ex) {
            LOG_ERROR("[id={}] [error={}] Failed to simulate program: {}", job_id, ex.id,
                      RemovePeriod(ex.what()));
        } catch (const std::exception& ex) {
            LOG_ERROR("[id={}] Failed to simulate program: {}", job_id, ex.what());
        }

        return MessageManager::GetMessage(INTERNAL_ERROR, fmt::arg("when", "simulating")).ToError();
    }

    std::optional<mqc3_cloud::common::v1::ErrorDetail> Upload(
        UploadResult* o_result, const std::string& job_id, const UploadSettings& settings,
        mqc3_cloud::scheduler::v1::JobResultUploadTarget upload_target,
        const mqc3_cloud::program::v1::QuantumProgramResult& result) {
        try {
            o_result->started_at = Timestamp::Now();

            std::string compressed;
            std::size_t raw_size_bytes = 0;
            std::size_t encoded_size_bytes = 0;

            if (!CompressByGzip(&compressed, &raw_size_bytes, &encoded_size_bytes, result)) {
                return MessageManager::GetMessage(INTERNAL_ERROR, fmt::arg("when", "uploading"))
                    .ToError();
            }

            for (std::uint32_t i = 0; i < settings.max_upload_retries; ++i) {
                RefreshUploadUrlIfNeeded(&upload_target, job_id, i);

                if (TryUploadBinary(upload_target.upload_url(), compressed)) {
                    o_result->uploaded_result.set_raw_size_bytes(raw_size_bytes);
                    o_result->uploaded_result.set_encoded_size_bytes(encoded_size_bytes);
                    o_result->finished_at = Timestamp::Now();
                    return std::nullopt;
                }

                LOG_WARNING("[id={}] Upload attempt {}/{} failed.", job_id, i + 1,
                            settings.max_upload_retries);

                std::this_thread::sleep_for(settings.upload_retry_interval_seconds);
            }

            // All retries failed - write compressed buffer to local backup.
            const auto backup_path =
                settings.backup_root_path / fmt::format("{}.out.proto.gz", job_id);

            if (!BackupBinary(backup_path, compressed)) {
                LOG_ERROR("[id={}] Failed to write backup output data: {}", job_id,
                          backup_path.string());
            }

            o_result->finished_at = Timestamp::Now();
        } catch (const std::exception& ex) {
            LOG_ERROR("[id={}] Failed to upload job result: {}", job_id, ex.what());
        }

        return MessageManager::GetMessage(INTERNAL_ERROR, fmt::arg("when", "uploading")).ToError();
    }

    void Report(const std::string& job_id,
                const mqc3_cloud::scheduler::v1::ReportExecutionResultRequest& report) {
        if (report.has_error()) {
            LOG_INFO("[id={}] Report error: {} ({})", job_id, report.error().description(),
                     report.error().code());
            log_.Error(report.error().code());
        } else {
            LOG_INFO("[id={}] Report result.", job_id);
            log_.Success();
        }

        for (std::uint32_t i = 0; i < settings_.max_report_retries; ++i) {
            try {
                auto response = execution_client_->ReportExecutionResult(report);
                if (response.error().code().empty()) {
                    // Successfully reported.
                    return;
                } else {
                    LOG_WARNING("[id={}] Failed to report result: {} ({})", job_id,
                                response.error().description(), response.error().code());
                }
            } catch (std::exception& e) {
                LOG_ERROR("[id={}] Exception occurred during ReportExecutionResult: {}", job_id,
                          e.what());
            }

            std::this_thread::sleep_for(settings_.report_retry_interval_seconds);
        }

        const auto backup_path =
            settings_.backup_root_path / fmt::format("{}.report.proto", job_id);
        if (!BackupBinary(backup_path, report.SerializeAsString())) {
            LOG_ERROR("[id={}] Failed to write backup report data: {}", job_id,
                      backup_path.string());
        }
    }

    const JobRunnerSettings settings_;
    ExecutionClient* const execution_client_;
    const mqc3_cloud::common::v1::ErrorDetail critical_error_;

    // Generate seed for each simulation.
    std::mt19937_64 engine_;

    // Report log.
    ReportLog log_;
};
