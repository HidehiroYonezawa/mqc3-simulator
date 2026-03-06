#pragma once

#include <cstdint>
#include <sys/resource.h>
#include <unordered_map>

#include "server/clients/scheduler.h"
#include "server/common/utility.h"
#include "server/runner/runner.h"
#include "server/runner/validate.h"

#include "mqc3_cloud/scheduler/v1/execution.pb.h"

struct ServerStatus {
    std::uint64_t n_requests = 0;
    std::uint64_t n_jobs = 0;
};

struct SimulatorServerSettings {
    std::vector<std::string> backends;

    std::chrono::duration<float> fetch_polling_interval;
    Duration python_env_reset_interval;
    Duration server_status_log_interval;

    // Runner settings.

    //   Validation settings.
    ValidateSettings validation;

    //  Upload settings.
    std::filesystem::path backup_root_path;
    std::uint32_t max_upload_retries;
    std::chrono::duration<float> upload_retry_interval_seconds;

    //  Report settings.
    std::uint32_t max_report_retries;
    std::chrono::duration<float> report_retry_interval_seconds;
};

class SimulatorServer {
public:
    using SimulatePrecision = double;

    SimulatorServer(const SimulatorServerSettings& settings, ExecutionClient* execution_client)
        : settings_{settings}, execution_client_{execution_client} {
        for (const auto& server_runner_backend : {"cpu", "single_gpu", "multi_gpu"}) {
            const auto job_runner_settings = JobRunnerSettings{
                .backend = server_runner_backend,
                .validation = settings.validation,
                .backup_root_path = settings_.backup_root_path,
                .max_upload_retries = settings_.max_report_retries,
                .upload_retry_interval_seconds = settings_.upload_retry_interval_seconds,
                .max_report_retries = settings_.max_report_retries,
                .report_retry_interval_seconds = settings_.report_retry_interval_seconds,
            };

            job_runner_container_.emplace_back(job_runner_settings, execution_client_);
        }

        const auto cpu_runner = job_runner_container_.begin();
        const auto single_gpu_runner = std::next(cpu_runner);
        const auto multi_gpu_runner = std::next(single_gpu_runner);

        // Set mapping from user defined backend to simulation server's backend.
        for (const auto& user_input_backend : settings_.backends) {
            if (user_input_backend == "cpu") {
                job_runner_mp_[user_input_backend] = &(*cpu_runner);
            } else if (user_input_backend == "single_gpu") {
                job_runner_mp_[user_input_backend] = &(*single_gpu_runner);
            } else if (user_input_backend == "multi_gpu") {
                job_runner_mp_[user_input_backend] = &(*multi_gpu_runner);
            }
        }
        job_runner_mp_["gpu"] = &(*multi_gpu_runner);
    }

    void Run();

private:
    bool IsSupportedBackend(const std::string& backend) const {
        return job_runner_mp_.contains(backend);
    }
    JobRunner<SimulatePrecision>& GetJobRunner(const std::string& backend) {
        return *job_runner_mp_[backend];
    }

    std::optional<mqc3_cloud::scheduler::v1::AssignNextJobResponse> FetchJob();
    void ReportUnknownBackend(const mqc3_cloud::scheduler::v1::AssignNextJobResponse& response);

    const SimulatorServerSettings settings_;
    ExecutionClient* const execution_client_;

    // Runner
    std::list<JobRunner<SimulatePrecision>> job_runner_container_;

    /**
     * @brief From user-input backend to job runner.
     */
    std::unordered_map<std::string, JobRunner<SimulatePrecision>*> job_runner_mp_;
};
