#pragma once

#include <optional>
#include <vector>

#include "bosim/base/log.h"
#include "bosim/operation.h"

namespace bosim {
struct GPUInfo {
    std::int32_t device;
    std::string name;
    std::int32_t major;
    std::int32_t minor;
    std::uint64_t total_global_mem;
};

#ifdef BOSIM_USE_GPU

std::optional<GPUInfo> GetGPUInfo(std::int32_t device);
std::vector<GPUInfo> GetGPUsInfo();
/**
 * @brief Uses the CUDA Runtime API to display information about available GPUs in the system.
 * * This function is only implemented when BOSIM_USE_GPU is defined.
 */
void PrintEnableGPUs();

#else

inline std::optional<GPUInfo> GetGPUInfo(std::int32_t device) {
    LOG_WARNING(
        "GPU support is disabled. Rebuild bosim with the BOSIM_USE_GPU option to enable CUDA "
        "features.");
    return std::nullopt;
}
inline std::vector<GPUInfo> GetGPUsInfo() {
    LOG_WARNING(
        "GPU support is disabled. Rebuild bosim with the BOSIM_USE_GPU option to enable CUDA "
        "features.");
    return {};
}
/**
 * @brief Uses the CUDA Runtime API to display information about available GPUs in the system.
 * * This function is only implemented when BOSIM_USE_GPU is defined.
 */
inline void PrintEnableGPUs() {
    LOG_WARNING(
        "GPU support is disabled. Rebuild bosim with the BOSIM_USE_GPU option to enable CUDA "
        "features.");
}

#endif
}  // namespace bosim
