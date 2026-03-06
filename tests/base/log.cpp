#include "bosim/base/log.h"

#include <gtest/gtest.h>

#include <regex>

TEST(Log, Macro) {
    bosim::Logger::EnableConsoleOutput();
    bosim::Logger::SetLogLevel(bosim::LogLevel::Debug);
    LOG_DEBUG("DEBUG");
    LOG_INFO("INFO");
    LOG_WARNING("WARNING");
    LOG_ERROR("ERROR");
    LOG_FATAL("FATAL");
}
TEST(Log, Disable) {
    bosim::Logger::DisableConsoleOutput();
    bosim::Logger::SetLogLevel(bosim::LogLevel::Debug);
    LOG_DEBUG("DEBUG");
    LOG_INFO("INFO");
    LOG_WARNING("WARNING");
    LOG_ERROR("ERROR");
    LOG_FATAL("FATAL");
}
TEST(Log, LogLevel) {
    bosim::Logger::EnableConsoleOutput();
    bosim::Logger::SetLogLevel(bosim::LogLevel::Error);
    LOG_DEBUG("DEBUG");
    LOG_INFO("INFO");
    LOG_WARNING("WARNING");
    LOG_ERROR("ERROR");
    LOG_FATAL("FATAL");
}
TEST(Log, Colorful) {
    bosim::Logger::EnableConsoleOutput();
    bosim::Logger::EnableColorfulOutput();
    bosim::Logger::SetLogLevel(bosim::LogLevel::Debug);
    LOG_DEBUG("DEBUG");
    LOG_INFO("INFO");
    LOG_WARNING("WARNING");
    LOG_ERROR("ERROR");
    LOG_FATAL("FATAL");
}

auto ReadFileToString(const std::filesystem::path& file_path) -> std::string {
    auto ifs = std::ifstream(file_path);
    if (!ifs) { throw std::runtime_error("file open error: " + file_path.string()); }
    auto ss = std::stringstream{};
    ss << ifs.rdbuf();
    return ss.str();
}

auto MakeLogLinePattern(std::string_view log_level, std::string_view msg) {
    return std::regex(
        fmt::format("^[-\\d]+ [:\\d]+ - {}\\s+- {} \\(log\\.cpp:\\d+\\)\n$", log_level, msg));
}

TEST(Log, FileOutputNoRotation) {
    const auto log_file_path = std::string("test_log_file_no_rotate");
    bosim::Logger::EnableFileOutput(log_file_path);
    bosim::Logger::SetLogLevel(bosim::LogLevel::Info);

    LOG_INFO("INFO");
    LOG_DEBUG("DEBUG");  // This should not be logged.
    EXPECT_TRUE(
        std::regex_match(ReadFileToString(log_file_path), MakeLogLinePattern("INFO", "INFO")));

    // Remove the test log file.
    std::filesystem::remove(log_file_path);
}

TEST(Log, FileOutputRotation) {
    const auto log_file_path = std::string("test_log_file_rotate");
    const auto rotate_file_path_1 = log_file_path + ".1";
    const auto rotate_file_path_2 = log_file_path + ".2";
    bosim::Logger::EnableFileOutput(log_file_path, 1, 2);
    bosim::Logger::SetLogLevel(bosim::LogLevel::Debug);

    LOG_FATAL("FATAL");
    EXPECT_TRUE(
        std::regex_match(ReadFileToString(log_file_path), MakeLogLinePattern("FATAL", "FATAL")));

    LOG_INFO("INFO");
    EXPECT_TRUE(
        std::regex_match(ReadFileToString(log_file_path), MakeLogLinePattern("INFO", "INFO")));
    EXPECT_TRUE(std::regex_match(ReadFileToString(rotate_file_path_1),
                                 MakeLogLinePattern("FATAL", "FATAL")));

    LOG_WARNING("WARNING");
    EXPECT_TRUE(std::regex_match(ReadFileToString(log_file_path),
                                 MakeLogLinePattern("WARNING", "WARNING")));
    EXPECT_TRUE(
        std::regex_match(ReadFileToString(rotate_file_path_1), MakeLogLinePattern("INFO", "INFO")));
    EXPECT_TRUE(std::regex_match(ReadFileToString(rotate_file_path_2),
                                 MakeLogLinePattern("FATAL", "FATAL")));

    LOG_ERROR("ERROR");
    EXPECT_TRUE(
        std::regex_match(ReadFileToString(log_file_path), MakeLogLinePattern("ERROR", "ERROR")));
    EXPECT_TRUE(std::regex_match(ReadFileToString(rotate_file_path_1),
                                 MakeLogLinePattern("WARNING", "WARNING")));
    EXPECT_TRUE(
        std::regex_match(ReadFileToString(rotate_file_path_2), MakeLogLinePattern("INFO", "INFO")));

    // Remove the test log files.
    std::filesystem::remove(log_file_path);
    std::filesystem::remove(rotate_file_path_1);
    std::filesystem::remove(rotate_file_path_2);
}
