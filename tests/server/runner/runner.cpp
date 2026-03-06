#include "server/runner/runner.h"

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <optional>

#include "server/clients/base.h"
#include "server/clients/scheduler.h"
#include "server/runner/upload.h"
#include "server/runner/validate.h"

TEST(JobRunner, Construct) {
    const auto execution_client_settings =
        rpc::GrpcClientSettings{.address = "localhost:8088",
                                .max_send_message_length = 64 * 1024 * 1024,
                                .max_receive_message_length = 64 * 1024 * 1024,
                                .max_retry_count = 3,
                                .timeout = std::chrono::milliseconds{30000},
                                .ca_cert_file = std::nullopt};
    auto execution_client = ExecutionClient(execution_client_settings);

    const auto job_runner_settings =
        JobRunnerSettings{.backend = "cpu",
                          .validation = ValidateSettings{},
                          .backup_root_path = std::filesystem::path("/var/tmp/backup"),
                          .max_upload_retries = 3,
                          .upload_retry_interval_seconds = std::chrono::duration<float>(1.0),
                          .max_report_retries = 3,
                          .report_retry_interval_seconds = std::chrono::duration<float>(1.0)};
    auto runner = JobRunner<double>(job_runner_settings, &execution_client);
}

TEST(Compress, Empty) {
    auto compressed = std::string();
    auto raw_size_bytes = std::size_t{0};
    auto encoded_size_bytes = std::size_t{0};
    auto result = mqc3_cloud::program::v1::QuantumProgramResult();
    const auto success = CompressByGzip(&compressed, &raw_size_bytes, &encoded_size_bytes, result);
    EXPECT_TRUE(success);
}
