#include <gtest/gtest.h>

#include <numbers>

#include "bosim/circuit.h"
#include "bosim/exception.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"

static constexpr auto Pi = std::numbers::pi_v<double>;
static constexpr auto SqueezingLevel = 100.0;

auto CreateTeleportationCircuit(double x, double p)
    -> std::pair<bosim::Circuit<double>, bosim::State<double>> {
    // Circuit
    auto circuit = bosim::Circuit<double>{};

    // BS
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, 0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(2, -Pi / 2.0);
    circuit.EmplaceOperation<bosim::operation::Manual<double>>(1, 2, 0, Pi / 2, Pi / 4,
                                                               3.0 * Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(2, Pi / 4.0);

    // BS
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, 0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 2.0);
    circuit.EmplaceOperation<bosim::operation::Manual<double>>(0, 1, 0, Pi / 2, Pi / 4,
                                                               3.0 * Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, -Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, Pi / 4.0);

    // Measure
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, Pi / 2);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(1, 0);
    circuit.EmplaceOperation<bosim::operation::Displacement<double>>(2, 0, 0);

    const auto ff_func_x =
        bosim::FFFunc<double>{[](const auto& x) -> double { return std::sqrt(2) * x[0]; }};
    const auto ff_func_p =
        bosim::FFFunc<double>{[](const auto& p) -> double { return -std::sqrt(2) * p[0]; }};
    auto ff_x = bosim::FeedForward<double>{{10}, {12, 0}, ff_func_x};
    circuit.AddFF(ff_x);
    auto ff_p = bosim::FeedForward<double>{{11}, {12, 1}, ff_func_p};
    circuit.AddFF(ff_p);

    // State
    auto state = bosim::State<double>{
        {bosim::SingleModeMultiPeak<double>::GenerateSqueezed(0, Pi / 2,
                                                              std::complex<double>{x, p}),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel, Pi / 2),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel)}};
    return {std::move(circuit), std::move(state)};
}

TEST(Circuit, Teleportation) {
    constexpr auto NShots = std::size_t{10};

    for (const auto backend :
         {bosim::Backend::CPUSingleThread, bosim::Backend::CPUMultiThreadShotLevel,
          bosim::Backend::CPUMultiThreadPeakLevel, bosim::Backend::CPUMultiThread}) {
        const auto settings = bosim::SimulateSettings{NShots, backend, bosim::StateSaveMethod::All};

        for (const auto& x : {1.5, -20.0, 0.01}) {
            for (const auto& p : {1.5, -20.0, -0.01}) {
                auto [circuit, state] = CreateTeleportationCircuit(x, p);
                const auto result = bosim::Simulate<double>(settings, circuit, state);
                for (std::size_t shot = 0; shot < NShots; ++shot) {
                    const auto& mean = result.result[shot].state.value().GetMean(0);
                    EXPECT_NEAR(x, mean(2).real(), 2 * 1e-5);
                    EXPECT_NEAR(p, mean(5).real(), 2 * 1e-5);
                }
            }
        }
    }
}

TEST(CPUSingleThread, Timeout) {
    const auto settings = bosim::SimulateSettings{.n_shots = 10,
                                                  .backend = bosim::Backend::CPUSingleThread,
                                                  .save_state_method = bosim::StateSaveMethod::All,
                                                  .timeout = std::chrono::seconds(-1)};
    auto [circuit, state] = CreateTeleportationCircuit(0, 0);
    EXPECT_THROW(bosim::Simulate<double>(settings, circuit, state), bosim::SimulationError);
}
TEST(CPUMultiThread, Timeout) {
    const auto settings = bosim::SimulateSettings{.n_shots = 10,
                                                  .backend = bosim::Backend::CPUMultiThread,
                                                  .save_state_method = bosim::StateSaveMethod::All,
                                                  .timeout = std::chrono::seconds(-1)};
    auto [circuit, state] = CreateTeleportationCircuit(0, 0);
    EXPECT_THROW(bosim::Simulate<double>(settings, circuit, state), bosim::SimulationError);
}
TEST(CPUMultiThreadShotLevel, Timeout) {
    const auto settings =
        bosim::SimulateSettings{.n_shots = 10,
                                .backend = bosim::Backend::CPUMultiThreadShotLevel,
                                .save_state_method = bosim::StateSaveMethod::All,
                                .timeout = std::chrono::seconds(-1)};
    auto [circuit, state] = CreateTeleportationCircuit(0, 0);
    EXPECT_THROW(bosim::Simulate<double>(settings, circuit, state), bosim::SimulationError);
}
TEST(CPUMultiThreadPeakLevel, Timeout) {
    const auto settings =
        bosim::SimulateSettings{.n_shots = 10,
                                .backend = bosim::Backend::CPUMultiThreadPeakLevel,
                                .save_state_method = bosim::StateSaveMethod::All,
                                .timeout = std::chrono::seconds(-1)};
    auto [circuit, state] = CreateTeleportationCircuit(0, 0);
    EXPECT_THROW(bosim::Simulate<double>(settings, circuit, state), bosim::SimulationError);
}
