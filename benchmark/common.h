#pragma once

#include "bosim/simulate/simulate.h"

namespace bosim::benchmark {
template <std::floating_point Float>
void Benchmark(const std::size_t n_trial, const std::size_t n_warmup,
               const bosim::SimulateSettings& settings, Circuit<Float>& circuit,
               State<Float>& state) {
    for (std::size_t i = 0; i < n_trial + n_warmup; ++i) {
        auto result = bosim::Simulate<Float>(settings, circuit, state);
        if (i < n_warmup) { bosim::profiler::TimeRecord::GetInstance().Clear(); }
    }
}
}  // namespace bosim::benchmark
