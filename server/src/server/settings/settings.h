#pragma once

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/resource.h>
#include <unordered_map>
#include <vector>

#include "server/common/utility.h"

struct Settings {
    // Environment
    std::string env = "debug";
    bool debug = true;
    std::string hostname = "localhost";

    // Application
    std::string app_name = "simulator";
    std::vector<std::string> backends = {"cpu", "gpu", "single_gpu", "multi_gpu"};
    std::string error_file = "server/settings/errors.json";

    // Logging
    std::string log_level = "INFO";
    bool log_color = false;
    std::string log_file = "app-run/local/log/bosim_server.log";
    std::size_t log_max_bytes = 0;
    std::size_t log_backup_count = 0;

    // Endpoint: execution service
    std::string execution_address = "localhost:8088";
    std::int32_t execution_max_send_message_length = 1024 * 1024 * 1024;
    std::int32_t execution_max_receive_message_length = 1024 * 1024 * 1024;
    std::uint32_t execution_max_retry_count = 3;
    std::chrono::milliseconds execution_timeout = std::chrono::milliseconds(120 * 1000);
    std::optional<std::string> execution_ca_cert_file = std::nullopt;

    // Simulator server settings
    std::chrono::duration<float> fetch_polling_interval = std::chrono::duration<float>(1.0);
    Duration python_env_reset_interval =
        Duration(TimeUtil::SecondsToDuration(std::int64_t{24} * 3600));                 // 1 day
    Duration server_status_log_interval = Duration(TimeUtil::SecondsToDuration(3600));  // 1 hour

    // Runner settings

    //   Validation settings.
    std::int32_t max_shots_when_save_all_states = 1'000;
    std::int32_t max_shots_otherwise = 1'000'000;
    std::uint64_t max_prod_shot_readout = 100'000'000;  // 100 MiB
    std::unordered_map<std::string, Duration> max_timeout_per_role = {
        {"admin", Duration(3600)},     // 1 hour
        {"developer", Duration(600)},  // 10 minutes
        {"guest", Duration(300)},      // 5 minutes
    };
    std::uint32_t max_n_modes = 20'000;
    std::uint32_t max_n_ops = 10'000'000;
    std::uint32_t max_n_measurement_ops = 20'000;
    std::uint32_t max_prod_n_peaks = 1'000'000;
    std::uint64_t max_memory_usage = 1'000'000'000;  // 1 GiB
    std::uint32_t max_n_local_macronodes_graph = 1'000;
    std::uint32_t max_n_steps_graph = 10'000;
    std::uint32_t max_n_local_macronodes_machinery = 1'000;
    std::uint32_t max_n_steps_machinery = 1'000;

    //   Upload settings.
    std::filesystem::path backup_root_path = "app-run/local/backup";
    std::uint32_t max_upload_retries = 3;
    std::chrono::duration<float> upload_retry_interval_seconds = std::chrono::duration<float>(1.0);

    //   Report setings.
    std::uint32_t max_report_retries = 3;
    std::chrono::duration<float> report_retry_interval_seconds = std::chrono::duration<float>(1.0);
};

inline void to_json(nlohmann::ordered_json& j, const Settings& s) {  // NOLINT
    // Environment
    j["env"] = s.env;
    j["debug"] = s.debug;
    j["hostname"] = s.hostname;

    // Application
    j["app_name"] = s.app_name;
    j["backends"] = s.backends;
    j["error_file"] = s.error_file;

    // Logging
    j["log_level"] = s.log_level;
    j["log_color"] = s.log_color;
    j["log_file"] = s.log_file;
    j["log_max_bytes"] = s.log_max_bytes;
    j["log_backup_count"] = s.log_backup_count;

    // Endpoint: execution service
    j["execution_address"] = s.execution_address;
    j["execution_max_send_message_length"] = s.execution_max_send_message_length;
    j["execution_max_receive_message_length"] = s.execution_max_receive_message_length;
    j["execution_max_retry_count"] = s.execution_max_retry_count;
    j["execution_timeout_ms"] = static_cast<std::int64_t>(s.execution_timeout.count());
    if (s.execution_ca_cert_file.has_value()) {
        j["execution_ca_cert_file"] = *s.execution_ca_cert_file;
    } else {
        j["execution_ca_cert_file"] = nullptr;
    }

    // Simulator server settings
    j["fetch_polling_interval_seconds"] = s.fetch_polling_interval.count();
    j["python_env_reset_interval"] = s.python_env_reset_interval.Seconds();
    j["server_status_log_interval"] = s.server_status_log_interval.Seconds();

    // Runner settings - Validation
    j["max_shots_when_save_all_states"] = s.max_shots_when_save_all_states;
    j["max_shots_otherwise"] = s.max_shots_otherwise;
    j["max_prod_shot_readout"] = s.max_prod_shot_readout;

    // max_timeout_per_role -> { role: seconds }
    {
        auto timeouts = nlohmann::ordered_json::object();
        for (const auto& [role, dur] : s.max_timeout_per_role) { timeouts[role] = dur.Seconds(); }
        j["max_timeout_per_role"] = std::move(timeouts);
    }

    j["max_n_modes"] = s.max_n_modes;
    j["max_n_ops"] = s.max_n_ops;
    j["max_n_measurement_ops"] = s.max_n_measurement_ops;
    j["max_prod_n_peaks"] = s.max_prod_n_peaks;
    j["max_memory_usage"] = s.max_memory_usage;
    j["max_n_local_macronodes_graph"] = s.max_n_local_macronodes_graph;
    j["max_n_steps_graph"] = s.max_n_steps_graph;
    j["max_n_local_macronodes_machinery"] = s.max_n_local_macronodes_machinery;
    j["max_n_steps_machinery"] = s.max_n_steps_machinery;

    // Upload settings
    j["backup_root_path"] = s.backup_root_path.string();
    j["max_upload_retries"] = s.max_upload_retries;
    j["upload_retry_interval_seconds"] = s.upload_retry_interval_seconds.count();

    // Report settings
    j["max_report_retries"] = s.max_report_retries;
    j["report_retry_interval_seconds"] = s.report_retry_interval_seconds.count();
}

inline void from_json(const nlohmann::ordered_json& j, Settings& s) {  // NOLINT
    auto set_if = [&](const char* key, auto&& setter) {
        if (j.contains(key) && !j.at(key).is_null()) setter(j.at(key));
    };

    // Environment
    set_if("env", [&](const nlohmann::json& v) { v.get_to(s.env); });
    set_if("debug", [&](const nlohmann::json& v) { v.get_to(s.debug); });
    set_if("hostname", [&](const nlohmann::json& v) { v.get_to(s.hostname); });

    // Application
    set_if("app_name", [&](const nlohmann::json& v) { v.get_to(s.app_name); });
    set_if("backends", [&](const nlohmann::json& v) { v.get_to(s.backends); });
    set_if("error_file", [&](const nlohmann::json& v) { v.get_to(s.error_file); });

    // Logging
    set_if("log_level", [&](const nlohmann::json& v) { v.get_to(s.log_level); });
    set_if("log_color", [&](const nlohmann::json& v) { v.get_to(s.log_color); });
    set_if("log_file", [&](const nlohmann::json& v) { v.get_to(s.log_file); });
    set_if("log_max_bytes", [&](const nlohmann::json& v) { v.get_to(s.log_max_bytes); });
    set_if("log_backup_count", [&](const nlohmann::json& v) { v.get_to(s.log_backup_count); });

    // Endpoint: execution service
    set_if("execution_address", [&](const nlohmann::json& v) { v.get_to(s.execution_address); });
    set_if("execution_max_send_message_length",
           [&](const nlohmann::json& v) { v.get_to(s.execution_max_send_message_length); });
    set_if("execution_max_receive_message_length",
           [&](const nlohmann::json& v) { v.get_to(s.execution_max_receive_message_length); });
    set_if("execution_max_retry_count",
           [&](const nlohmann::json& v) { v.get_to(s.execution_max_retry_count); });
    set_if("execution_timeout_ms", [&](const nlohmann::json& v) {
        std::int64_t ms = v.get<std::int64_t>();
        s.execution_timeout = std::chrono::milliseconds(ms);
    });
    set_if("execution_ca_cert_file", [&](const nlohmann::json& v) {
        if (v.is_null()) {
            s.execution_ca_cert_file.reset();
        } else {
            s.execution_ca_cert_file = v.get<std::string>();
        }
    });

    // Simulator server settings
    set_if("fetch_polling_interval_seconds", [&](const nlohmann::json& v) {
        s.fetch_polling_interval = std::chrono::duration<float>(v.get<float>());
    });

    set_if("python_env_reset_interval", [&](const nlohmann::json& v) {
        s.python_env_reset_interval = Duration(v.get<double>());
    });
    set_if("server_status_log_interval", [&](const nlohmann::json& v) {
        s.server_status_log_interval = Duration(v.get<double>());
    });

    // Runner settings - Validation
    set_if("max_shots_when_save_all_states",
           [&](const nlohmann::json& v) { v.get_to(s.max_shots_when_save_all_states); });
    set_if("max_shots_otherwise",
           [&](const nlohmann::json& v) { v.get_to(s.max_shots_otherwise); });
    set_if("max_prod_shot_readout",
           [&](const nlohmann::json& v) { v.get_to(s.max_prod_shot_readout); });

    set_if("max_timeout_per_role", [&](const nlohmann::json& v) {
        auto tmp = std::unordered_map<std::string, Duration>{};
        for (auto it = v.begin(); it != v.end(); ++it) {
            tmp.emplace(it.key(), Duration(it.value().get<double>()));
        }
        s.max_timeout_per_role = std::move(tmp);
    });

    set_if("max_n_modes", [&](const nlohmann::json& v) { v.get_to(s.max_n_modes); });
    set_if("max_n_ops", [&](const nlohmann::json& v) { v.get_to(s.max_n_ops); });
    set_if("max_n_measurement_ops",
           [&](const nlohmann::json& v) { v.get_to(s.max_n_measurement_ops); });
    set_if("max_prod_n_peaks", [&](const nlohmann::json& v) { v.get_to(s.max_prod_n_peaks); });
    set_if("max_memory_usage", [&](const nlohmann::json& v) { v.get_to(s.max_memory_usage); });
    set_if("max_n_local_macronodes_graph",
           [&](const nlohmann::json& v) { v.get_to(s.max_n_local_macronodes_graph); });
    set_if("max_n_steps_graph", [&](const nlohmann::json& v) { v.get_to(s.max_n_steps_graph); });
    set_if("max_n_local_macronodes_machinery",
           [&](const nlohmann::json& v) { v.get_to(s.max_n_local_macronodes_machinery); });
    set_if("max_n_steps_machinery",
           [&](const nlohmann::json& v) { v.get_to(s.max_n_steps_machinery); });

    // Upload settings
    set_if("backup_root_path", [&](const nlohmann::json& v) {
        s.backup_root_path = std::filesystem::path(v.get<std::string>());
    });
    set_if("max_upload_retries", [&](const nlohmann::json& v) { v.get_to(s.max_upload_retries); });
    set_if("upload_retry_interval_seconds", [&](const nlohmann::json& v) {
        s.upload_retry_interval_seconds = std::chrono::duration<float>(v.get<float>());
    });

    // Report settings
    set_if("max_report_retries", [&](const nlohmann::json& v) { v.get_to(s.max_report_retries); });
    set_if("report_retry_interval_seconds", [&](const nlohmann::json& v) {
        s.report_retry_interval_seconds = std::chrono::duration<float>(v.get<float>());
    });
}
