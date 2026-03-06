#include "bosim/gpu/simulate_gpu.h"

#include <fmt/base.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>

#include "bosim/exception.h"
#include "bosim/gpu/simulator_gpu.cuh"
#include "bosim/gpu/state_gpu.cuh"
#include "bosim/gpu/status.h"
#include "bosim/result.h"
#include "bosim/simulate/settings.h"
#include "bosim/state.h"

namespace bosim::gpu {

template <std::floating_point Float>
void SimulateSingleGPUImpl(const std::uint64_t seed, const std::uint64_t shot,
                           const Circuit<Float>& circuit, const StateGPU<Float>& original_state,
                           const bool save_state, SimulatorGPU<Float>& simulator,
                           cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateSingleGPUImpl>{shot};
#endif

#ifdef BOSIM_BUILD_BENCHMARK
    auto copy_profile = profiler::Profiler<profiler::Section::CopyStateGPU>{shot};
#endif

    auto local_state = original_state;

#ifdef BOSIM_BUILD_BENCHMARK
    copy_profile.End();
#endif

    local_state.SetSeed(seed);
    local_state.SetShot(shot);

    auto local_circuit = circuit.Copy();
    simulator.Execute(local_state, local_circuit, stream);
    if (save_state) {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::DownloadStateToCPU>{};
#endif
        simulator.GetMutShotResult(shot).state = local_state.ToState();
    }
}

std::uint32_t EstimateNThreads(const std::uint64_t total_global_mem,
                               const std::uint64_t state_bytes) {
    if (state_bytes == 0) { throw OtherError(error::Unreachable, "Bytes of state is zero."); }
    if (total_global_mem < 2 * state_bytes) {
        throw SimulationError(error::InvalidState, "State is too large to simulate on GPU.");
    }

    return std::clamp(static_cast<std::uint32_t>(total_global_mem / state_bytes) -
                          1,  // 1 for original state (when simulating, copy state from original)
                      1u, 100u);
}

template <std::floating_point Float, typename Clock, typename Duration>
void SimulateSingleGPUInternal(const GPUInfo& info_gpu, const EndTime<Clock, Duration>& end_time,
                               const SimulateSettings& settings, const Circuit<Float>& circuit,
                               SimulatorGPU<Float>& simulator, const StateGPU<Float>& state_gpu,
                               const std::size_t shot_start, const std::size_t shot_end) {
    if (settings.backend == Backend::SingleGPUSingleThread ||
        settings.backend == Backend::MultiGPUSingleThread) {
        cudaStream_t stream;
        cudaStreamCreate(&stream);
        for (auto i = shot_start; i < shot_end; ++i) {
            RaiseErrorIfTimedout(end_time);

            const auto save_state =
                settings.save_state_method == StateSaveMethod::All ||
                (settings.save_state_method == StateSaveMethod::FirstOnly && i == 0u);
            SimulateSingleGPUImpl(settings.seed + i, i, circuit, state_gpu, save_state, simulator,
                                  stream);
            cudaStreamSynchronize(stream);
        }
        cudaStreamDestroy(stream);
    } else {
        // Backend: SingleGPU or SingleGPUMultiThread
        const auto n_threads = EstimateNThreads(info_gpu.total_global_mem, state_gpu.Bytes());
        const auto n_shots_per_thread = (shot_end - shot_start) / n_threads;
        const auto n_additional_shots = (shot_end - shot_start) % n_threads;

        auto futures = std::vector<std::future<void>>{};
        futures.reserve(n_threads);

        auto local_shot_start = shot_start;
        for (auto t = 0u; t < n_threads; ++t) {
            const auto n_local_shots = n_shots_per_thread + (t < n_additional_shots ? 1u : 0u);
            const auto local_shot_end = local_shot_start + n_local_shots;

            futures.emplace_back(std::async(
                std::launch::async, [&info_gpu, &end_time, &settings, &circuit, &state_gpu,
                                     &simulator, local_shot_start, local_shot_end]() {
                    cudaSetDevice(info_gpu.device);
                    cudaStream_t stream;
                    cudaStreamCreate(&stream);

                    for (auto shot = local_shot_start; shot < local_shot_end; ++shot) {
                        RaiseErrorIfTimedout(end_time);

                        const auto save_state =
                            settings.save_state_method == StateSaveMethod::All ||
                            (settings.save_state_method == StateSaveMethod::FirstOnly &&
                             shot == 0u);
                        SimulateSingleGPUImpl(settings.seed + shot, shot, circuit, state_gpu,
                                              save_state, simulator, stream);
                        cudaStreamSynchronize(stream);
                    }

                    cudaStreamDestroy(stream);
                }));
            local_shot_start = local_shot_end;
        }
        for (auto& future : futures) { future.get(); }
    }
}

template <typename Float>
std::size_t EstimateDeviceMemBytes(const std::size_t n_modes, const std::size_t n_peaks) {
    return sizeof(Float) * n_peaks *
           (2 +                            // coeff (2 = complex)
            2 * (2 * n_modes) +            // mean  (2 = complex)
            (2 * n_modes) * (2 * n_modes)  // cov
           );
}
template <typename Float>
std::size_t EstimateDeviceMemBytes(const State<Float>& state) {
    const auto n_modes = state.NumModes();
    const auto n_peaks = state.NumPeaks();
    return EstimateDeviceMemBytes<Float>(n_modes, n_peaks);
}
template <typename Float>
std::size_t EstimateDeviceMemBytes(const std::vector<SingleModeMultiPeak<Float>>& modes) {
    const auto n_modes = modes.size();
    std::uint64_t n_peaks = 1;

    for (const auto& mode : modes) {
        n_peaks *= mode.peaks.size();
        if (MaxProductNPeaks < n_peaks) {
            throw SimulationError(
                error::InvalidState,
                fmt::format("Product of peaks of each mode exceeds the allowed limit ({}).",
                            MaxProductNPeaks));
        }
    }

    return EstimateDeviceMemBytes<Float>(n_modes, n_peaks);
}

void RaiseErrorIfOOM(const std::size_t total_global_mem, const std::size_t estimate) {
    // GPU simulation allocates at least two states (original and copy to simulate).
    // est / 10 is for buffer. (TODO: estimate more precisely)
    if (total_global_mem < 2 * estimate + estimate / 10) {
        throw SimulationError(
            error::InvalidState,
            fmt::format("State is too large to simulate on GPU (total global mem={}).",
                        total_global_mem));
    }
}

GPUInfo SetDeviceSingleGPU(const std::vector<std::int32_t>& devices) {
    if (devices.empty()) {
        const auto info_all = GetGPUsInfo();
        if (!info_all.empty()) { return info_all.front(); }
    } else {
        for (const auto device : devices) {
            auto info = GetGPUInfo(device);
            if (info.has_value()) {
                cudaSetDevice(device);
                return info.value();
            }
        }
    }
    throw SimulationError(error::InvalidSettings,
                          fmt::format("No valid device in settings: {}", devices));
}

template <typename Float, typename HostState>
Result<Float> SimulateSingleGPUCommon(const SimulateSettings& settings,
                                      const Circuit<Float>& circuit, const HostState& state) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateSingleGPU>{};
#endif
    const auto begin = std::chrono::high_resolution_clock::now();
    const auto end_time =
        settings.timeout ? std::make_optional(begin + settings.timeout.value()) : std::nullopt;

    const auto info_gpu = SetDeviceSingleGPU(settings.devices);
    {
        const auto estimate = EstimateDeviceMemBytes(state);
        RaiseErrorIfOOM(info_gpu.total_global_mem, estimate);
    }

#ifdef BOSIM_BUILD_BENCHMARK
    auto upload_profile = profiler::Profiler<profiler::Section::UploadStateToGPU>{};
#endif

    auto state_gpu = StateGPU(state);

#ifdef BOSIM_BUILD_BENCHMARK
    upload_profile.End();
#endif

    auto simulator = SimulatorGPU<Float>{settings.n_shots};
    SimulateSingleGPUInternal(info_gpu, end_time, settings, circuit, simulator, state_gpu, 0,
                              settings.n_shots);

    const auto end = std::chrono::high_resolution_clock::now();
    simulator.GetMutResult().elapsed_ms = std::chrono::duration_cast<Ms>(end - begin);
    return simulator.GetResult();
}

Result<double> SimulateSingleGPU(const SimulateSettings& settings, const Circuit<double>& circuit,
                                 const State<double>& state) {
    return SimulateSingleGPUCommon<double>(settings, circuit, state);
}
Result<float> SimulateSingleGPU(const SimulateSettings& settings, const Circuit<float>& circuit,
                                const State<float>& state) {
    return SimulateSingleGPUCommon<float>(settings, circuit, state);
}
Result<double> SimulateSingleGPU(const SimulateSettings& settings, const Circuit<double>& circuit,
                                 const std::vector<SingleModeMultiPeak<double>>& modes) {
    return SimulateSingleGPUCommon<double>(settings, circuit, modes);
}
Result<float> SimulateSingleGPU(const SimulateSettings& settings, const Circuit<float>& circuit,
                                const std::vector<SingleModeMultiPeak<float>>& modes) {
    return SimulateSingleGPUCommon<float>(settings, circuit, modes);
}

template <typename Float, typename HostState>
Result<Float> SimulateMultiGPUCommon(const SimulateSettings& settings,
                                     const Circuit<Float>& circuit, const HostState& state) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateMultiGPU>{};
#endif
    const auto begin = std::chrono::high_resolution_clock::now();
    const auto end_time =
        settings.timeout ? std::make_optional(begin + settings.timeout.value()) : std::nullopt;

    // List up enable devices.
    auto enable_devices = std::vector<std::int32_t>{};

    if (settings.devices.empty()) {
        const auto info_all = GetGPUsInfo();
        for (const auto& device : info_all) { enable_devices.emplace_back(device.device); }
    } else {
        for (const auto device : settings.devices) {
            if (GetGPUInfo(device).has_value()) { enable_devices.emplace_back(device); }
        }

        if (enable_devices.empty()) {
            throw SimulationError(error::InvalidSettings,
                                  fmt::format("No valid device in settings: {}", settings.devices));
        }
    }

    // Define simulator.
    auto simulator = SimulatorGPU<Float>{settings.n_shots};

    // Split shots into multiple devices.
    const auto n_devices = enable_devices.size();
    const auto shots_per_thread = settings.n_shots / n_devices;
    const auto num_additional_shots = settings.n_shots % n_devices;

    auto futures = std::vector<std::future<void>>{};
    futures.reserve(n_devices);

    auto shot_start = std::size_t{0};
    for (auto i = std::size_t{0}; i < enable_devices.size(); ++i) {
        const auto device = enable_devices[i];

        const auto n_local_shots = shots_per_thread + (i < num_additional_shots ? 1u : 0u);
        const auto shot_end = shot_start + n_local_shots;

        futures.emplace_back(std::async(
            std::launch::async,
            [&end_time, &settings, &circuit, &state, &simulator, device, shot_start, shot_end]() {
                const auto info_gpu = SetDeviceSingleGPU({device});
                {
                    const auto estimate = EstimateDeviceMemBytes(state);
                    RaiseErrorIfOOM(info_gpu.total_global_mem, estimate);
                }

#ifdef BOSIM_BUILD_BENCHMARK
                auto upload_profile = profiler::Profiler<profiler::Section::UploadStateToGPU>{};
#endif

                auto state_gpu = StateGPU(state);

#ifdef BOSIM_BUILD_BENCHMARK
                upload_profile.End();
#endif
                SimulateSingleGPUInternal(info_gpu, end_time, settings, circuit, simulator,
                                          state_gpu, shot_start, shot_end);
            }));
        shot_start = shot_end;
    }
    for (auto& future : futures) { future.get(); }

    const auto end = std::chrono::high_resolution_clock::now();
    simulator.GetMutResult().elapsed_ms = std::chrono::duration_cast<Ms>(end - begin);
    return simulator.GetResult();
}

Result<double> SimulateMultiGPU(const SimulateSettings& settings, const Circuit<double>& circuit,
                                const State<double>& state) {
    return SimulateMultiGPUCommon(settings, circuit, state);
}
Result<float> SimulateMultiGPU(const SimulateSettings& settings, const Circuit<float>& circuit,
                               const State<float>& state) {
    return SimulateMultiGPUCommon(settings, circuit, state);
}
Result<double> SimulateMultiGPU(const SimulateSettings& settings, const Circuit<double>& circuit,
                                const std::vector<SingleModeMultiPeak<double>>& modes) {
    return SimulateMultiGPUCommon(settings, circuit, modes);
}
Result<float> SimulateMultiGPU(const SimulateSettings& settings, const Circuit<float>& circuit,
                               const std::vector<SingleModeMultiPeak<float>>& modes) {
    return SimulateMultiGPUCommon(settings, circuit, modes);
}
}  // namespace bosim::gpu
