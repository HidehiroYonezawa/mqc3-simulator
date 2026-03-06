#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "bosim/circuit.h"
#include "bosim/simulate/simulate.h"

auto TeleportationConfig() -> std::pair<bosim::Circuit<double>, bosim::State<double>> {
    constexpr double SqueezingLevel = 10.0;
    constexpr auto Pi = std::numbers::pi_v<double>;
    constexpr auto Phi = std::numbers::pi_v<double> / 3;
    const auto t0 = (Pi + Phi) / 2;
    const auto t1 = Phi / 2;

    // Circuit
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::util::BeamSplitter50<double>>(1, 2);
    circuit.EmplaceOperation<bosim::operation::util::BeamSplitter50<double>>(0, 1);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, t0);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(1, t1);
    circuit.EmplaceOperation<bosim::operation::Displacement<double>>(2, 0, 0);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(2, Pi / 2 - Phi);

    const double coeff = -std::sqrt(2) / std::sin(t1 - t0);
    auto ff_func_x = bosim::FFFunc<double>{[coeff, t0, t1](const auto& m) {
        return coeff * (std::cos(t1) * m[0] + std::cos(t0) * m[1]);
    }};
    auto ff_func_p = bosim::FFFunc<double>{[coeff, t0, t1](const auto& m) {
        return coeff * (std::sin(t1) * m[0] + std::sin(t0) * m[1]);
    }};
    auto ff_x = bosim::FeedForward<double>{{2, 3}, {4, 0}, ff_func_x};
    circuit.AddFF(ff_x);
    auto ff_p = bosim::FeedForward<double>{{2, 3}, {4, 1}, ff_func_p};
    circuit.AddFF(ff_p);

    // State
    auto state = bosim::State<double>{
        {bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel, Pi / 2),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel)}};

    return {std::move(circuit), std::move(state)};
}

TEST(Parallel, Teleportation) {
    auto [circuit, state] = TeleportationConfig();

    constexpr std::size_t NShots = 100;
    const auto settings_single = bosim::SimulateSettings{NShots, bosim::Backend::CPUSingleThread,
                                                         bosim::StateSaveMethod::None};
    const auto settings_multi_shot_level = bosim::SimulateSettings{
        NShots, bosim::Backend::CPUMultiThreadShotLevel, bosim::StateSaveMethod::None};
    const auto settings_multi_peak_level = bosim::SimulateSettings{
        NShots, bosim::Backend::CPUMultiThreadPeakLevel, bosim::StateSaveMethod::None};
    const auto settings_multi = bosim::SimulateSettings{NShots, bosim::Backend::CPUMultiThread,
                                                        bosim::StateSaveMethod::None};
#ifdef BOSIM_USE_GPU
    const auto settings_single_gpu =
        bosim::SimulateSettings{NShots, bosim::Backend::SingleGPU, bosim::StateSaveMethod::None};
#endif
    auto result_single = bosim::Simulate<double>(settings_single, circuit, state);
    auto result_multi_shot_level =
        bosim::Simulate<double>(settings_multi_shot_level, circuit, state);
    auto result_multi_peak_level =
        bosim::Simulate<double>(settings_multi_peak_level, circuit, state);
    auto result_multi = bosim::Simulate<double>(settings_multi, circuit, state);
#ifdef BOSIM_USE_GPU
    auto result_single_gpu = bosim::Simulate<double>(settings_single_gpu, circuit, state);
#endif
    for (std::size_t shot = 0; shot < NShots; ++shot) {
        EXPECT_DOUBLE_EQ(result_single.result[shot].measured_values[5],
                         result_multi_shot_level.result[shot].measured_values[5]);
        EXPECT_DOUBLE_EQ(result_single.result[shot].measured_values[5],
                         result_multi_peak_level.result[shot].measured_values[5]);
        EXPECT_DOUBLE_EQ(result_single.result[shot].measured_values[5],
                         result_multi.result[shot].measured_values[5]);
#ifdef BOSIM_USE_GPU
        EXPECT_NEAR(result_single.result[shot].measured_values[5],
                    result_single_gpu.result[shot].measured_values[5], 1e-9);
#endif
    }
}
