#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include "bosim/exception.h"

namespace bosim {
/**
 * @brief Backend of simulator.
 */
enum class Backend : std::uint8_t {
    CPUSingleThread,          ///< Execute on CPU with a single thread.
    CPUMultiThread,           ///< Execute on CPU with optimized multi-threading.
    CPUMultiThreadPeakLevel,  ///< Execute on CPU with peak-level multi-threading.
    CPUMultiThreadShotLevel,  ///< Execute on CPU with shot-level multi-threading.
    SingleGPU,                ///< Execute on a single GPU with optimal backend.
    SingleGPUSingleThread,    ///< Execute on a single GPU with a single thread.
    SingleGPUMultiThread,     ///< Execute on a single GPU with shot-level multi-threading.
    MultiGPU,                 ///< Execute on multiple GPUs with optimal backend.
    MultiGPUSingleThread,     ///< Execute on multiple GPUs with a single thread.
    MultiGPUMultiThread,      ///< Execute on multiple GPUs with shot-level multi-threading.
};

/**
 * @brief Enum specifying how the final state of a simulation is retained.
 */
enum class StateSaveMethod : std::uint8_t {
    None,       ///< Do not retain the final state.
    FirstOnly,  ///< Retain only the first shot's final state.
    All         ///< Retain the final state for all shots.
};

struct SimulateSettings {
    std::uint32_t n_shots;
    Backend backend;
    StateSaveMethod save_state_method;

    std::uint64_t seed = 0;
    std::optional<std::chrono::seconds> timeout = std::nullopt;

    /**
     * @brief List of device IDs to be used for simulation.
     *
     * When the backend is configured for single GPU execution, the simulation will
     * use the first enabled device from this list. If the list is empty, it will
     * automatically find and use the first available enabled device, starting from
     * the lowest device ID.
     *
     *  For multi GPU execution, all enabled devices specified in this list will be
     * used. If the list is empty, all available enabled devices on the system
     * will be utilized.
     */
    std::vector<std::int32_t> devices = {};
};

template <typename Clock, typename Duration>
using EndTime = std::optional<std::chrono::time_point<Clock, Duration>>;

template <typename Clock, typename Duration>
void RaiseErrorIfTimedout(const EndTime<Clock, Duration>& end_time) {
    if (end_time.has_value() && end_time.value() < std::chrono::high_resolution_clock::now()) {
        throw SimulationError(error::Timeout,
                              "The simulation timed out. Please try again with a longer "
                              "timeout setting or a smaller number of shots.");
    }
}
}  // namespace bosim
