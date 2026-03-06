#pragma once

#include "bosim/circuit.h"
#include "bosim/operation.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"
#include "bosim/state.h"

namespace bosim::benchmark {
template <std::floating_point Float>
bosim::State<Float> InitialState(const std::size_t n_modes, const std::size_t n_cats) {
    auto gen = std::mt19937{0};  // NOLINT(cert-msc32-c, cert-msc51-cpp)
    auto angle_dist = std::uniform_real_distribution<Float>{-std::numbers::pi_v<Float>,
                                                            std::numbers::pi_v<Float>};

    const std::size_t n_squeezed_states = n_modes - n_cats;
    auto modes = std::vector<bosim::SingleModeMultiPeak<Float>>{};
    for (std::size_t i = 0; i < n_cats; ++i) {
        modes.push_back(bosim::SingleModeMultiPeak<Float>::GenerateCat(
            std::complex<Float>{std::polar(1.0, angle_dist(gen))}, 0));
    }
    for (std::size_t i = 0; i < n_squeezed_states; ++i) {
        modes.push_back(bosim::SingleModeMultiPeak<Float>::GenerateSqueezed(
            5, angle_dist(gen), std::complex<Float>{0, 0}));
    }

    auto state = bosim::State<Float>{modes};
    auto mode_idxs = std::vector<std::size_t>(n_modes);
    std::iota(mode_idxs.begin(), mode_idxs.end(), 0);
    std::shuffle(mode_idxs.begin(), mode_idxs.end(), gen);

    auto circuit = bosim::Circuit<double>{};
    for (const auto mode : mode_idxs) {
        if (mode == n_modes - 1) continue;
        circuit.EmplaceOperation<bosim::operation::util::BeamSplitterStd<Float>>(
            mode, mode + 1, angle_dist(gen), angle_dist(gen));
    }

    const auto settings = bosim::SimulateSettings{1, bosim::Backend::CPUMultiThread,
                                                  bosim::StateSaveMethod::FirstOnly};
    auto result = bosim::Simulate<Float>(settings, circuit, state);

    return result.result[0].state.value();
}
}  // namespace bosim::benchmark
