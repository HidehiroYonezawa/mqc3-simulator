#include "server/clients/base.h"
#include "server/clients/scheduler.h"
#include "server/runner/validate.h"
#include "server/server/server.h"
#include "server/settings/settings.h"

std::int32_t RunGRPCServer(const Settings& settings) {
    // Construct clients.
    const auto execution_client_settings = rpc::GrpcClientSettings{
        .address = settings.execution_address,
        .max_send_message_length = settings.execution_max_send_message_length,
        .max_receive_message_length = settings.execution_max_receive_message_length,
        .max_retry_count = settings.execution_max_retry_count,
        .timeout = settings.execution_timeout,
        .ca_cert_file = settings.execution_ca_cert_file};
    auto execution_client = ExecutionClient(execution_client_settings);

    // Construct server.
    const auto simulator_server_settings = SimulatorServerSettings{
        .backends = settings.backends,

        .fetch_polling_interval = settings.fetch_polling_interval,
        .python_env_reset_interval = settings.python_env_reset_interval,
        .server_status_log_interval = settings.server_status_log_interval,

        // Runner settings
        //  Validation settings.
        .validation =
            ValidateSettings{
                .max_shots_when_save_all_states = settings.max_shots_when_save_all_states,
                .max_shots_otherwise = settings.max_shots_otherwise,
                .max_prod_shot_readout = settings.max_prod_shot_readout,
                .max_timeout_per_role = settings.max_timeout_per_role,
                .max_n_modes = settings.max_n_modes,
                .max_n_ops = settings.max_n_ops,
                .max_n_measurement_ops = settings.max_n_measurement_ops,
                .max_prod_n_peaks = settings.max_prod_n_peaks,
                .max_memory_usage = settings.max_memory_usage,
                .max_n_local_macronodes_graph = settings.max_n_local_macronodes_graph,
                .max_n_steps_graph = settings.max_n_steps_graph,
                .max_n_local_macronodes_machinery = settings.max_n_local_macronodes_machinery,
                .max_n_steps_machinery = settings.max_n_steps_machinery
                //
            },

        //  Upload settings.
        .backup_root_path = settings.backup_root_path,
        .max_upload_retries = settings.max_upload_retries,
        .upload_retry_interval_seconds = settings.upload_retry_interval_seconds,

        //  Report settings.
        .max_report_retries = settings.max_report_retries,
        .report_retry_interval_seconds = settings.report_retry_interval_seconds};
    auto server = SimulatorServer(simulator_server_settings, &execution_client);

    server.Run();

    return 0;
}
