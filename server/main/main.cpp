#include <boost/program_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <nlohmann/json_fwd.hpp>

#include <fstream>

#include "bosim/base/log.h"

#include "server/common/message_manager.h"
#include "server/common/string.h"
#include "server/server/run_server.h"
#include "server/settings/settings.h"

int main(int argc, const char** argv) noexcept {
    namespace po = boost::program_options;

    // Define description.
    // clang-format off
    auto description = po::options_description(R"(bosim_server options)");
    description.add_options()
        ("help,h", "Display available options")
        // Settings.
        ("settings", po::value<std::string>(),"Path to the settings json file")
        // Error file.
        ("error_file", po::value<std::string>(), "Path to the error defining json")
        // Log options.
        ("log_level", po::value<std::string>(), "Verbosity level (0: QUIET, 1: WARN, 2: INFO, 3: DEBUG)")
        ("log_color", "Colorize output")
        ("log_file", po::value<std::string>(), "Path to log file")
        ("log_max_bytes", po::value<std::size_t>(), "Maximum size of log file before rotation (0 means unlimited)")
        ("log_backup_count", po::value<std::size_t>(), "Number of backup log files to keep")
        // Server options.
        ("execution_address", po::value<std::string>(), "Address to the execution service of the scheduler")
        ("execution_ca_cert_file", po::value<std::string>(), "Path to the credentials file to use the execution service")
        ("backup_root_path", po::value<std::string>(), "Path to the root directory of backup files")
    ; // NOLINT
    // clang-format on

    auto vm = po::variables_map();
    try {
        po::store(po::parse_command_line(argc, argv, description), vm);
        po::notify(vm);
    } catch (const po::error_with_option_name& ex) {
        std::cerr << ex.what() << std::endl;
        std::cerr << "To get the list of available options, run 'bosim_server --help'."
                  << std::endl;
        return 1;
    }

    if (vm.contains("help")) {
        std::cout << description;
        return 0;
    }

    // Setup settings.
    auto settings = Settings();
    if (vm.contains("settings")) {
        auto ifs = std::ifstream(vm["settings"].as<std::string>());
        auto j = nlohmann::ordered_json::parse(ifs);
        from_json(j, settings);
    }
    // Apply CLI argument overrides.
    if (vm.contains("error_file")) { settings.error_file = vm["error_file"].as<std::string>(); }
    if (vm.contains("log_level")) { settings.log_level = vm["log_level"].as<std::string>(); }
    if (vm.contains("log_color")) { settings.log_color = true; }
    if (vm.contains("log_file")) { settings.log_file = vm["log_file"].as<std::string>(); }
    if (vm.contains("log_max_bytes")) {
        settings.log_max_bytes = vm["log_max_bytes"].as<std::size_t>();
    }
    if (vm.contains("log_backup_count")) {
        settings.log_backup_count = vm["log_backup_count"].as<std::size_t>();
    }
    if (vm.contains("execution_address")) {
        settings.execution_address = vm["execution_address"].as<std::string>();
    }
    if (vm.contains("execution_ca_cert_file")) {
        settings.execution_ca_cert_file = vm["execution_ca_cert_file"].as<std::string>();
    }
    if (vm.contains("backup_root_path")) {
        settings.backup_root_path = vm["backup_root_path"].as<std::string>();
    }

    // Setup logger.
    bosim::Logger::EnableConsoleOutput();
    bosim::Logger::EnableFileOutput(settings.log_file, settings.log_max_bytes,
                                    settings.log_backup_count);
    if (ToUpper(settings.log_level) == "DEBUG") {
        bosim::Logger::SetLogLevel(bosim::LogLevel::Debug);
    } else if (ToUpper(settings.log_level) == "WARNING") {
        bosim::Logger::SetLogLevel(bosim::LogLevel::Warning);
    } else {
        bosim::Logger::SetLogLevel(bosim::LogLevel::Info);
    }
    if (settings.log_color) {
        bosim::Logger::EnableColorfulOutput();
    } else {
        bosim::Logger::DisableColorfulOutput();
    }

    // Setup MessageManager.
    MessageManager::InitializeFromFile(settings.error_file);

    // Setup directories.
    {
        // Log.
        const auto log_file = std::filesystem::path(settings.log_file);
        const auto log_dir = log_file.parent_path();
        if (!log_dir.empty()) {
            auto ec = std::error_code{};
            std::filesystem::create_directories(log_dir, ec);
            if (ec) {
                std::cerr << fmt::format("Failed to create log directory: {} ({})", ec.message(),
                                         ec.category().name())
                          << std::endl;
            }
        }
    }
    {
        // Backup.
        const auto backup_dir = std::filesystem::path(settings.backup_root_path);
        if (!backup_dir.empty()) {
            auto ec = std::error_code{};
            std::filesystem::create_directories(backup_dir, ec);
            if (ec) {
                std::cerr << fmt::format("Failed to create backup directory: {} ({})", ec.message(),
                                         ec.category().name())
                          << std::endl;
            }
        }
    }

    // Run server.
    LOG_INFO("Settings: {}", nlohmann::ordered_json(settings).dump());
    RunGRPCServer(settings);

    return 0;
}
