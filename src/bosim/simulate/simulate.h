#pragma once

#include "bosim/circuit.h"
#include "bosim/exception.h"
#include "bosim/result.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate_cpu_multi_thread.h"
#include "bosim/simulate/simulate_cpu_single_thread.h"
#include "bosim/state.h"

#ifdef BOSIM_BUILD_BENCHMARK
#include "bosim/base/timer.h"
#endif

#ifdef BOSIM_USE_GPU
#include "bosim/gpu/simulate_gpu.h"
#endif

namespace bosim {
/**
 * @brief Select simulation method when CPUMultiThread backend is selected.
 *
 * @details If the number of CPU cores `c` is one or less, the simulation runs on a single thread.
 *
 * For multi-core environment (`c > 1`), this function dynamically selects between two
 * parallelization strategies to maximize efficiency:
 * - **Peak-level parallelization:** Used for a small number of shots, distributing the peaks
 * across multiple threads.
 * - **Shot-level parallelization:** Used for a large number of shots, assigning each shot to a
 * separate thread.
 *
 * The selection is based on a linear model describing the benefit of peak-level parallelization.
 * The benefit `y` is modeled as a linear function of the number of shots `x`: `y = ax + b`.
 * The benefit is maximal (`y = p`, where `p` is the number of peaks) when `x = 1` and approaches
 * zero (`y = 0`) as `x` approaches the number of cores (`c`).
 *
 * Peak-level parallelization is only performed if the calculated benefit exceeds a defined
 * threshold `t`. This condition is satisfied when the number of shots is below
 * `c - t * (c - 1) / p`.
 * Otherwise, shot-level parallelization is selected.
 *
 * @tparam Float
 * @param settings Simulation settings.
 * @param circuit Circuit object.
 * @param state State object.
 * @return Backend Simulation execution backend.
 */
template <std::floating_point Float>
Backend CPUMultiThreadSelection(const SimulateSettings& settings, const State<Float>& state) {
    constexpr double BenefitThreshold = 4;
    const auto hardware_concurrency = std::thread::hardware_concurrency();
    if (hardware_concurrency <= 1) { return Backend::CPUSingleThread; }

    if (state.NumPeaks() == 0) {
        throw SimulationError(error::InvalidState, "No peaks in the state.");
    }
    if (settings.n_shots < hardware_concurrency - BenefitThreshold * (hardware_concurrency - 1) /
                                                      static_cast<double>(state.NumPeaks())) {
        return Backend::CPUMultiThreadPeakLevel;  // peak level
    }
    return Backend::CPUMultiThreadShotLevel;  // shot level
}

template <std::floating_point Float>
Result<Float> Simulate(const SimulateSettings& settings, Circuit<Float>& circuit,
                       const State<Float>& state) {
    if (settings.n_shots == 0) {
        throw SimulationError(error::InvalidSettings,
                              "The number of shots must be greater than 0.");
    }

#ifdef BOSIM_BUILD_BENCHMARK
    profiler::TimeRecord::GetInstance().Resize(settings.n_shots, state.NumPeaks());
#endif

    switch (settings.backend) {
        case Backend::CPUSingleThread: return SimulateCPUSingleThread(settings, circuit, state);
        case Backend::CPUMultiThread: {
            const auto backend = CPUMultiThreadSelection(settings, state);
            switch (backend) {
                case Backend::CPUSingleThread:
                    return SimulateCPUSingleThread(settings, circuit, state);
                case Backend::CPUMultiThreadPeakLevel:
                    return SimulateCPUMultiThread(settings, circuit, state, false);
                case Backend::CPUMultiThreadShotLevel:
                    return SimulateCPUMultiThread(settings, circuit, state, true);
                default:
                    throw OtherError(
                        error::Unreachable,
                        "CPUMultiThreadSelection function only returns CPUSingleThread, "
                        "CPUMultiThreadPeakLevel, or CPUMultiThreadShotLevel.");
            }
        }
        case Backend::CPUMultiThreadPeakLevel:
            return SimulateCPUMultiThread(settings, circuit, state, false);
        case Backend::CPUMultiThreadShotLevel:
            return SimulateCPUMultiThread(settings, circuit, state, true);
        case Backend::SingleGPU:
        case Backend::SingleGPUSingleThread:
        case Backend::SingleGPUMultiThread: {
#ifdef BOSIM_USE_GPU
            return gpu::SimulateSingleGPU(settings, circuit, state);
#else
            throw SimulationError(
                error::InvalidSettings,
                fmt::format("GPU backend requested, but BOSIM was built without GPU support. "
                            "Please rebuild with BOSIM_USE_GPU=ON or select a CPU backend."));
#endif
        }
        case Backend::MultiGPU:
        case Backend::MultiGPUSingleThread:
        case Backend::MultiGPUMultiThread: {
#ifdef BOSIM_USE_GPU
            return gpu::SimulateMultiGPU(settings, circuit, state);
#else
            throw SimulationError(
                error::InvalidSettings,
                fmt::format("GPU backend requested, but BOSIM was built without GPU support. "
                            "Please rebuild with BOSIM_USE_GPU=ON or select a CPU backend."));
#endif
        }
        default: throw OtherError(error::Unreachable, "Unknown backend.");
    }
}

template <std::floating_point Float>
Result<Float> Simulate(const SimulateSettings& settings, Circuit<Float>& circuit,
                       const std::vector<SingleModeMultiPeak<Float>>& modes) {
    if (settings.n_shots == 0) {
        throw SimulationError(error::InvalidSettings,
                              "The number of shots must be greater than 0.");
    }

#ifdef BOSIM_BUILD_BENCHMARK
    auto sizes = std::vector<std::size_t>{};
    std::transform(modes.begin(), modes.end(), std::back_inserter(sizes),
                   [](const auto& x) { return x.peaks.size(); });
    const std::size_t n_peaks =
        std::accumulate(sizes.begin(), sizes.end(), 1u, std::multiplies<>());

    profiler::TimeRecord::GetInstance().Resize(settings.n_shots, n_peaks);
#endif

    switch (settings.backend) {
        case Backend::CPUSingleThread:
            return SimulateCPUSingleThread(settings, circuit, State(modes));
        case Backend::CPUMultiThread: {
            const auto state = State(modes);
            const auto backend = CPUMultiThreadSelection(settings, state);
            switch (backend) {
                case Backend::CPUSingleThread:
                    return SimulateCPUSingleThread(settings, circuit, state);
                case Backend::CPUMultiThreadPeakLevel:
                    return SimulateCPUMultiThread(settings, circuit, state, false);
                case Backend::CPUMultiThreadShotLevel:
                    return SimulateCPUMultiThread(settings, circuit, state, true);
                default:
                    throw OtherError(
                        error::Unreachable,
                        "CPUMultiThreadSelection function only returns CPUSingleThread, "
                        "CPUMultiThreadPeakLevel, or CPUMultiThreadShotLevel.");
            }
        }
        case Backend::CPUMultiThreadPeakLevel:
            return SimulateCPUMultiThread(settings, circuit, State(modes), false);
        case Backend::CPUMultiThreadShotLevel:
            return SimulateCPUMultiThread(settings, circuit, State(modes), true);
        case Backend::SingleGPU:
        case Backend::SingleGPUSingleThread:
        case Backend::SingleGPUMultiThread: {
#ifdef BOSIM_USE_GPU
            return gpu::SimulateSingleGPU(settings, circuit, modes);
#else
            throw SimulationError(
                error::InvalidSettings,
                fmt::format("GPU backend requested, but BOSIM was built without GPU support. "
                            "Please rebuild with BOSIM_USE_GPU=ON or select a CPU backend."));
#endif
        }
        case Backend::MultiGPU:
        case Backend::MultiGPUSingleThread:
        case Backend::MultiGPUMultiThread: {
#ifdef BOSIM_USE_GPU
            return gpu::SimulateMultiGPU(settings, circuit, modes);
#else
            throw SimulationError(
                error::InvalidSettings,
                fmt::format("GPU backend requested, but BOSIM was built without GPU support. "
                            "Please rebuild with BOSIM_USE_GPU=ON or select a CPU backend."));
#endif
        }
        default: throw OtherError(error::Unreachable, "Unknown backend.");
    }
}
}  // namespace bosim
