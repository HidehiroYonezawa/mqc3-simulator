#include "server/server/server.h"

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <atomic>
#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <exception>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <sys/resource.h>
#include <thread>

#include "bosim/base/log.h"
#include "bosim/gpu/status.h"
#include "bosim/version.h"

#include "server/common/error_code.h"
#include "server/common/message_manager.h"
#include "server/common/stats.h"
#include "server/common/utility.h"
#include "server/runner/runner.h"
#include "server/settings/token.h"

#include "mqc3_cloud/scheduler/v1/execution.pb.h"

// ---------------- Graceful shutdown ----------------
namespace {
std::atomic_bool g_shutdown{false};

void SignalHandler(int) { g_shutdown.store(true, std::memory_order_relaxed); }

void InstallSignalHandlers() {
    struct sigaction sa;
    sa.sa_handler = SignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}
}  // namespace

// ---------------- Memory utilities (Linux) ----------------
inline double KiBToMiB(std::size_t kib) { return static_cast<double>(kib) / 1024.0; }

std::size_t GetPeakRssKiB() {
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) { return static_cast<std::size_t>(usage.ru_maxrss); }
    return 0;
}

// Current RSS (KiB) from /proc/self/status (VmRSS)
std::size_t GetCurrentRssKiB() noexcept {
    std::ifstream ifs("/proc/self/status");
    if (!ifs) return 0;

    std::string line;
    while (std::getline(ifs, line)) {
        // Expect a line like: "VmRSS:	   12345 kB"
        if (line.starts_with("VmRSS:")) {
            // Find the first digit in the line.
            const std::size_t pos = line.find_first_of("0123456789");
            if (pos == std::string::npos) return 0;

            // Parse an unsigned integer starting at the first digit.
            const char* begin = line.data() + pos;
            const char* end = line.data() + line.size();

            unsigned long long value = 0;
            const auto res = std::from_chars(begin, end, value);  // base-10 by default
            if (res.ec == std::errc{}) {
                // Clamp to size_t if necessary
                if (value > std::numeric_limits<std::size_t>::max()) {
                    return std::numeric_limits<std::size_t>::max();
                }
                return static_cast<std::size_t>(value);
            }
            // Parsing failed
            return 0;
        }
    }
    return 0;
}

// ---------------- Logging ----------------
void LogServerStatus(
    const ServerStatus& status,
    std::list<JobRunner<SimulatorServer::SimulatePrecision>>& job_runner_container) {
    const auto cur_rss_mib = KiBToMiB(GetCurrentRssKiB());
    const auto peak_rss_mib = KiBToMiB(GetPeakRssKiB());
    LOG_INFO("Server status: requests={}, jobs={}, mem.current={:.2f}[MiB], mem.peak={:.2f}[MiB]",
             status.n_requests, status.n_jobs, cur_rss_mib, peak_rss_mib);

    auto report_log = ReportLog();
    for (auto& job_runner : job_runner_container) {
        report_log.Merge(job_runner.GetLog());
        job_runner.ClearLog();
    }
    LOG_INFO("{}", report_log.ToString());
}

// ---------------- SimulatorServer ----------------
void SimulatorServer::Run() {
    InstallSignalHandlers();
    LOG_INFO("SimulatorServer starting. supported_backends={}", settings_.backends);

    bosim::PrintEnableGPUs();

    auto status = ServerStatus{};
    auto next_server_status_log_time = Timestamp::Now();
    auto python_env = std::make_unique<bosim::python::PythonEnvironment>();
    auto next_python_env_reset_time = Timestamp::Now() + settings_.python_env_reset_interval;

    while (!g_shutdown.load(std::memory_order_relaxed)) {
        try {
            const auto now = Timestamp::Now();

            // Periodic Python environment reset.
            if (now >= next_python_env_reset_time) {
                LOG_INFO("Resetting Python environment.");
                python_env.reset();
                python_env = std::make_unique<bosim::python::PythonEnvironment>();
                next_python_env_reset_time = now + settings_.python_env_reset_interval;
            }

            // Periodic server status log.
            if (now >= next_server_status_log_time) {
                LogServerStatus(status, job_runner_container_);
                status = ServerStatus{};
                next_server_status_log_time = now + settings_.server_status_log_interval;
            }

            // Fetch next job.
            const auto job = FetchJob();
            status.n_requests++;

            if (!job) {
                // No job available: poll at a fixed interval.
                std::this_thread::sleep_for(settings_.fetch_polling_interval);
                continue;
            }

            status.n_jobs++;

            const auto& job_id = job->job_id();
            const auto& backend = job->job().settings().backend();

            LOG_INFO("[id={}] Starting job: backend={}, n_shots={}, timeout={:.2f}[sec]", job_id,
                     backend, job->job().settings().n_shots(),
                     Duration(job->job().settings().timeout()).Seconds());

            if (!IsSupportedBackend(backend)) {
                ReportUnknownBackend(job.value());
                continue;
            }

            // Run job.
            GetJobRunner(backend).Run(job.value());

            LOG_INFO("[id={}] Finished job (backend={})", job_id, backend);
        } catch (const std::exception& ex) {
            LOG_ERROR("Unexpected exception in main loop: {}", ex.what());
            // Sleep 10 seconds.
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }

    LOG_INFO("SimulatorServer shutting down gracefully.");
}

std::optional<mqc3_cloud::scheduler::v1::AssignNextJobResponse> SimulatorServer::FetchJob() {
    try {
        auto request = mqc3_cloud::scheduler::v1::AssignNextJobRequest();
        request.set_token(GetToken());
        request.set_backend("all");
        auto response = execution_client_->AssignNextJob(request);
        if (response.has_job()) { return response; }
    } catch (const std::exception& ex) {
        LOG_WARNING("Exception during AssignNextJob: {}", ex.what());
    }
    return std::nullopt;
}

void SimulatorServer::ReportUnknownBackend(
    const mqc3_cloud::scheduler::v1::AssignNextJobResponse& response) {
    try {
        const auto& job_id = response.job_id();
        const auto& backend = response.job().settings().backend();

        auto report = mqc3_cloud::scheduler::v1::ReportExecutionResultRequest();
        report.set_token(GetToken());
        report.set_job_id(job_id);
        report.set_status(mqc3_cloud::scheduler::v1::ExecutionStatus::EXECUTION_STATUS_FAILURE);

        const auto reason = fmt::format("Unknown backend: {}. Supported backends are: {}", backend,
                                        settings_.backends);
        report.mutable_error()->CopyFrom(
            MessageManager::GetMessage(INVALID_PROGRAM, fmt::arg("reason", reason)).ToError());

        report.mutable_version()->set_simulator(bosim::GetVersion());

        const auto report_result = execution_client_->ReportExecutionResult(report);
        if (report_result.has_error()) {
            LOG_ERROR("[id={}] Failed to report unknown backend: {} ({})", job_id,
                      report_result.error().description(), report_result.error().code());
        } else {
            LOG_INFO("[id={}] Reported unknown backend successfully.", job_id);
        }
    } catch (const std::exception& ex) {
        LOG_ERROR("[id={}] Exception during ReportExecutionResult: {}", response.job_id(),
                  ex.what());
    }
}
