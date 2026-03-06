#include "bosim/gpu/status.h"

#include <cuda_runtime.h>

#include <cstdio>

namespace bosim {
#ifdef BOSIM_USE_GPU
std::optional<GPUInfo> GetGPUInfo(std::int32_t device) {
    cudaDeviceProp dev_prop;
    cudaError_t error_id = cudaGetDeviceProperties(&dev_prop, device);

    // Error handling for individual device property retrieval
    if (error_id != cudaSuccess) {
        LOG_ERROR("cudaGetDeviceProperties for device {} failed: {}", device,
                  cudaGetErrorString(error_id));
        return std::nullopt;
    }

    return GPUInfo{.device = device,
                   .name = std::string(dev_prop.name),
                   .major = dev_prop.major,
                   .minor = dev_prop.minor,
                   .total_global_mem = dev_prop.totalGlobalMem};
}

std::vector<GPUInfo> GetGPUsInfo() {
    int device_count = 0;

    // 1. Get the number of CUDA devices
    cudaError_t error_id = cudaGetDeviceCount(&device_count);

    if (error_id != cudaSuccess) {
        // Error handling for device count retrieval
        LOG_ERROR("cudaGetDeviceCount failed: {}", cudaGetErrorString(error_id));
        return {};
    }

    // 2. Enumerate and display properties for each device
    auto ret = std::vector<GPUInfo>{};
    for (int i = 0; i < device_count; ++i) {
        auto info = GetGPUInfo(i);
        if (info.has_value()) { ret.emplace_back(info.value()); }
    }
    return ret;
}

void PrintEnableGPUs() {
    auto infos = GetGPUsInfo();

    if (infos.size() == 0) {
        LOG_INFO("Found 0 NVIDIA devices supporting CUDA.");
    } else {
        LOG_INFO("Found {} GPU(s) with CUDA support. Details below:", infos.size());
    }

    for (const auto& info : infos) {
        LOG_INFO("--- Device {} ---", info.device);
        LOG_INFO("  Name: {}", info.name);

        // Calculate memory in GB.
        double total_global_memory_gb =
            static_cast<double>(info.total_global_mem) / (1024.0 * 1024.0 * 1024.0);
        LOG_INFO("  Total Global Memory: {} GB", total_global_memory_gb);

        // Combine compute capability into a string (e.g., "8.6").
        std::string compute_capability =
            std::to_string(info.major) + "." + std::to_string(info.minor);
        LOG_INFO("  Compute Capability: {}", compute_capability);
    }
}

#endif  // BOSIM_USE_GPU
}  // namespace bosim
