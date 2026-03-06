#pragma once

#include <fmt/format.h>

#include <string>
#include <unordered_map>

struct ReportLog {
    std::uint32_t success = 0;
    std::unordered_map<std::string, std::uint32_t> failure = {};

    void Clear() {
        success = 0;
        failure = {};
    }

    void Success() { success++; }
    void Error(const std::string& code) {
        const auto itr = failure.find(code);
        if (itr == failure.end()) {
            failure[code] = 1;
        } else {
            itr->second++;
        }
    }

    void Merge(const ReportLog& log) {
        success += log.success;
        for (const auto& [code, n] : log.failure) {
            const auto itr = failure.find(code);
            if (itr == failure.end()) {
                failure[code] = n;
            } else {
                itr->second += n;
            }
        }
    }

    std::string ToString() const {
        auto n_failed = std::uint32_t{0};
        for (const auto& [code, n] : failure) { n_failed += n; }
        auto failure_report = fmt::format("failure={}", n_failed);

        if (n_failed > 0) {
            failure_report += " (";
            auto first = true;
            for (const auto& [code, n] : failure) {
                if (first) {
                    failure_report += fmt::format("{}={}", code, n);
                    first = false;
                } else {
                    failure_report += fmt::format(" {}={}", code, n);
                }
            }
            failure_report += ")";
        }

        return fmt::format("success={} {}", success, failure_report);
    }
};
