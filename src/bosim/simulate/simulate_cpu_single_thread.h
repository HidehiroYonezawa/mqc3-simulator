#pragma once

#include <optional>

#include "bosim/base/log.h"
#include "bosim/circuit.h"
#include "bosim/result.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulator.h"
#include "bosim/state.h"

namespace bosim {
template <std::floating_point Float>
void SimulateCPUSingleThreadImpl(const std::uint64_t seed, const std::uint64_t shot,
                                 Circuit<Float>& circuit, const State<Float>& state,
                                 const bool save_state, Simulator<Float>& simulator) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateCPUSingleThreadImpl>{shot};
#endif
    auto state_copy = state;
    state_copy.SetSeed(seed);
    state_copy.SetShot(shot);
    simulator.Execute(state_copy, circuit);
    if (save_state) { simulator.GetMutShotResult(shot).state = std::move(state_copy); }
}
template <std::floating_point Float>
Result<Float> SimulateCPUSingleThread(const SimulateSettings& settings, Circuit<Float>& circuit,
                                      const State<Float>& state) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateCPUSingleThread>{};
#endif
    const auto begin = std::chrono::high_resolution_clock::now();
    const auto end_time =
        settings.timeout ? std::make_optional(begin + settings.timeout.value()) : std::nullopt;

    const auto n_shots = settings.n_shots;
    auto simulator = Simulator<Float>{n_shots};

    for (auto i = 0u; i < n_shots; ++i) {
        LOG_DEBUG("Running {}(/{})-th simulation.", i + 1, n_shots);
        RaiseErrorIfTimedout(end_time);

        const auto save_state =
            settings.save_state_method == StateSaveMethod::All ||
            (settings.save_state_method == StateSaveMethod::FirstOnly && i == 0u);
        SimulateCPUSingleThreadImpl(settings.seed + i, i, circuit, state, save_state, simulator);
    }

    const auto end = std::chrono::high_resolution_clock::now();
    simulator.GetMutResult().elapsed_ms = std::chrono::duration_cast<Ms>(end - begin);
    return simulator.GetResult();
}
}  // namespace bosim
