#pragma once

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <string>

inline std::string ReadTokenFile(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) { throw std::runtime_error("Failed to open token file: " + path); }

    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) { s.pop_back(); }

    if (s.empty()) { throw std::runtime_error("Token file is empty."); }
    return s;
}

inline const std::string& GetToken() {
    static const std::string Cached = []() {
        if (const char* env = std::getenv("SCHEDULER_TOKEN")) {
            if (*env != '\0') return std::string(env);
        }
        if (const char* p = std::getenv("SCHEDULER_TOKEN_FILE")) {
            if (*p != '\0') return ReadTokenFile(p);
        }
        const std::string default_path = "/run/secrets/scheduler_token";
        return ReadTokenFile(default_path);
    }();

    return Cached;
}
