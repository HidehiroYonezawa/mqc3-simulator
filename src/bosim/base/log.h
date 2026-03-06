#pragma once

#include <fmt/format.h>

#include <array>
#include <chrono>  // NOLINT
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace bosim {
/**
 * @brief Log level.
 */
enum class LogLevel : std::uint8_t {
    Fatal = 0,
    Error = 1,
    Warning = 2,
    Info = 3,
    Debug = 4,
};
/**
 * @brief Logger class intended for use as a singleton.
 */
struct Logger {
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    static void Log(LogLevel level, std::string_view filename, std::int32_t line,
                    std::string_view msg) {
        GetLogger()->LogConsoleImpl(level, msg, filename, line);
        GetLogger()->LogFileImpl(level, msg, filename, line);
    }

    // Set option
    static void SetLogLevel(LogLevel new_log_level) { GetLogger()->log_level_ = new_log_level; }
    static void DisableColorfulOutput() { GetLogger()->color_ = false; }
    static void EnableColorfulOutput() { GetLogger()->color_ = true; }
    static void DisableConsoleOutput() { GetLogger()->console_output_ = false; }
    static void EnableConsoleOutput() { GetLogger()->console_output_ = true; }
    static void DisableFileOutput() { GetLogger()->file_output_ = false; }
    static void EnableFileOutput(const std::string& path, const std::size_t max_bytes = 0,
                                 const std::size_t backup_count = 0) {
        GetLogger()->file_output_ = true;
        GetLogger()->ofs_ = std::ofstream(path);
        if (!GetLogger()->ofs_.good()) { throw std::runtime_error("file open error: " + path); }
        GetLogger()->log_file_path_ = std::filesystem::path(path);
        GetLogger()->max_bytes_per_file_ = max_bytes;
        GetLogger()->backup_count_ = backup_count;
    }

    // Get option
    [[nodiscard]] static bool Verbose() { return OutputToConsole() || OutputToFile(); }
    [[nodiscard]] static bool OutputToConsole() { return GetLogger()->console_output_; }
    [[nodiscard]] static bool OutputToFile() { return GetLogger()->file_output_; }
    [[nodiscard]] static LogLevel GetLogLevel() { return GetLogger()->log_level_; }

private:
    Logger() = default;
    ~Logger() = default;

    /**
     * @brief Get a pointer to the singleton Logger.
     */
    static Logger* GetLogger();

    static std::string GetLogPrefix(LogLevel level, bool color) {
        using namespace std::chrono;
        static constexpr auto Name =
            std::array<const char*, 5>{"FATAL  ", "ERROR  ", "WARNING", "INFO   ", "DEBUG  "};
        static constexpr auto ColorfulName = std::array<const char*, 5>{
            "\033[1;31mFATAL\033[0m  ",  // Light Red     1;31
            "\033[0;31mERROR\033[0m  ",  // Red           0;31
            "\033[1;33mWARNING\033[0m",  // Yellow        1;33
            "\033[0;32mINFO\033[0m   ",  // Green         0;32
            "\033[0;37mDEBUG\033[0m  "   // Light Gray    0;37
        };

        const auto t = system_clock::to_time_t(system_clock::now());
        const auto& lt = *std::localtime(&t);
        return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02} - {} - ",  // Format
                           lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min,
                           lt.tm_sec,  // Date
                           color ? ColorfulName[static_cast<std::size_t>(level)]
                                 : Name[static_cast<std::size_t>(level)]  // Log level
        );
    }
    static std::string GetLogSuffix(std::string_view filename, std::int32_t line) {
        return fmt::format(" ({}:{})", filename, line);
    }
    bool ShouldRollover(LogLevel level, const std::size_t bytes_to_write) {
        if (max_bytes_per_file_ == 0 || backup_count_ == 0) { return false; }
        if (!file_output_ || !ofs_ || !std::filesystem::exists(log_file_path_)) { return false; }
        if (level > log_level_) { return false; }

        return log_file_bytes_ + bytes_to_write > max_bytes_per_file_;
    }
    auto GetRotationFilePath(const std::size_t i) {
        if (i == 0) { return log_file_path_; }
        return std::filesystem::path(log_file_path_.string() + "." + std::to_string(i));
    }
    void DoRollover() {
        if (ofs_) { ofs_.close(); }

        if (backup_count_ > 0) {
            for (auto i = backup_count_; i >= 1; --i) {
                const auto src_path = GetRotationFilePath(i - 1);
                const auto dst_path = GetRotationFilePath(i);
                if (std::filesystem::exists(src_path)) {
                    std::filesystem::rename(src_path, dst_path);
                }
            }
        }

        log_file_bytes_ = 0;
        ofs_ = std::ofstream(log_file_path_);
        if (!ofs_.good()) {
            throw std::runtime_error("file open error: " + log_file_path_.string());
        }
    }
    void LogConsoleImpl(LogLevel level, std::string_view msg, std::string_view filename,
                        std::int32_t line) {
        if (!console_output_) { return; }
        if (level > log_level_) { return; }
        std::cout << GetLogPrefix(level, color_) << msg << GetLogSuffix(filename, line)
                  << std::endl;
    }
    void LogFileImpl(LogLevel level, std::string_view msg, std::string_view filename,
                     std::int32_t line) {
        if (!file_output_) { return; }
        if (!ofs_) { return; }
        if (level > log_level_) { return; }
        const auto log_prefix = GetLogPrefix(level, false);
        const auto log_suffix = GetLogSuffix(filename, line);
        const auto bytes_to_write = log_prefix.size() + msg.size() + 1;  // +1 for "\n"
        if (ShouldRollover(level, bytes_to_write)) { DoRollover(); }
        ofs_ << log_prefix << msg << log_suffix << std::endl;
        log_file_bytes_ += bytes_to_write;
    }

    LogLevel log_level_ = LogLevel::Info;
    bool color_ = false;
    bool console_output_ = false;
    bool file_output_ = false;
    std::ofstream ofs_;
    std::filesystem::path log_file_path_;
    std::size_t log_file_bytes_ = 0;
    std::size_t max_bytes_per_file_ = 0;
    std::size_t backup_count_ = 0;
};
}  // namespace bosim

#ifndef __FILE_NAME__
#include <string.h>  // strrchr
#define __FILE_NAME__                                    \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 \
                            : (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__))
#endif

/**
 * @brief Log a message at the FATAL level.
 */
#define LOG_FATAL(MSG, ...)                                                 \
    do {                                                                    \
        bosim::Logger::Log(bosim::LogLevel::Fatal, __FILE_NAME__, __LINE__, \
                           fmt::format(MSG __VA_OPT__(, ) __VA_ARGS__));    \
    } while (0)
/**
 * @brief Log a message at the ERROR level.
 */
#define LOG_ERROR(MSG, ...)                                                 \
    do {                                                                    \
        bosim::Logger::Log(bosim::LogLevel::Error, __FILE_NAME__, __LINE__, \
                           fmt::format(MSG __VA_OPT__(, ) __VA_ARGS__));    \
    } while (0)
/**
 * @brief Log a message at the WARNING level.
 */
#define LOG_WARNING(MSG, ...)                                                 \
    do {                                                                      \
        bosim::Logger::Log(bosim::LogLevel::Warning, __FILE_NAME__, __LINE__, \
                           fmt::format(MSG __VA_OPT__(, ) __VA_ARGS__));      \
    } while (0)
/**
 * @brief Log a message at the INFO level.
 */
#define LOG_INFO(MSG, ...)                                                 \
    do {                                                                   \
        bosim::Logger::Log(bosim::LogLevel::Info, __FILE_NAME__, __LINE__, \
                           fmt::format(MSG __VA_OPT__(, ) __VA_ARGS__));   \
    } while (0)
/**
 * @brief Log a message at the DEBUG level.
 * @details This macro is only available in DEBUG build.
 */
#ifndef NDEBUG
#define LOG_DEBUG(MSG, ...)                                                 \
    do {                                                                    \
        bosim::Logger::Log(bosim::LogLevel::Debug, __FILE_NAME__, __LINE__, \
                           fmt::format(MSG __VA_OPT__(, ) __VA_ARGS__));    \
    } while (0)
#else
#define LOG_DEBUG(...) \
    do {               \
    } while (0)
#endif
/**
 * @brief Log a message at the DEBUG level.
 */
#define LOG_DEBUG_ALWAYS(MSG, ...)                                          \
    do {                                                                    \
        bosim::Logger::Log(bosim::LogLevel::Debug, __FILE_NAME__, __LINE__, \
                           fmt::format(MSG __VA_OPT__(, ) __VA_ARGS__));    \
    } while (0)
