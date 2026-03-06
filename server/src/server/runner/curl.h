#pragma once

#include <atomic>
#include <cstdlib>  // std::atexit
#include <curl/curl.h>
#include <mutex>

#include "bosim/base/log.h"

namespace curl_global {
/**
 * Thread-safe, process-wide libcurl initialization helper.
 * Ensures curl_global_init() runs exactly once; schedules curl_global_cleanup()
 * at process exit via std::atexit(). Returns true on success.
 */
inline std::once_flag once;
inline std::atomic<bool> ok{false};
inline std::atomic<int> init_code{CURLE_OK};

inline void CurlGlobalCleanup() noexcept { curl_global_cleanup(); }

inline bool EnsureInitialized() {
    std::call_once(once, [] {
        const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
        init_code.store(static_cast<int>(rc), std::memory_order_relaxed);
        if (rc == CURLE_OK) {
            ok.store(true, std::memory_order_release);
            const int reg = std::atexit(&CurlGlobalCleanup);
            if (reg != 0) {
                LOG_WARNING(
                    "std::atexit registration failed; curl_global_cleanup() will not run at exit.");
            }
        } else {
            ok.store(false, std::memory_order_release);
        }
    });
    return ok.load(std::memory_order_acquire);
}

inline const char* LastErrorStr() {
    return curl_easy_strerror(static_cast<CURLcode>(init_code.load()));
}
}  // namespace curl_global
