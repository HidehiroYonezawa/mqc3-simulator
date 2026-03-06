#include <cmath>
#include <iostream>
#include <numbers>

#include "bosim/circuit.h"
#include "bosim/simulate/simulate.h"

static constexpr auto Pi = std::numbers::pi_v<double>;
static constexpr auto SqueezingLevel = 100.0;
static constexpr auto Threshold = 1e-5;

static constexpr std::string_view Reset = "\033[0m";
static constexpr std::string_view Green = "\033[32m";
static constexpr std::string_view Yellow = "\033[33m";

auto CreateTeleportationCircuit(double x, double p)
    -> std::pair<bosim::Circuit<double>, bosim::State<double>> {
    // Create the quantum circuit.
    auto circuit = bosim::Circuit<double>{};

    // Apply beam splitters.
    circuit.EmplaceOperation<bosim::operation::util::BeamSplitterStd<double>>(1, 2, Pi / 4, Pi);
    circuit.EmplaceOperation<bosim::operation::util::BeamSplitterStd<double>>(0, 1, Pi / 4, Pi);

    // Perform homodyne measurements.
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, Pi / 2);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(1, 0);

    // The initial displacement (0,0) will be updated later via feedforward.
    circuit.EmplaceOperation<bosim::operation::Displacement<double>>(2, 0, 0);

    // Define feedforward functions.
    const auto ff_func_x =
        bosim::FFFunc<double>{[](const auto& x) -> double { return std::sqrt(2) * x[0]; }};
    const auto ff_func_p =
        bosim::FFFunc<double>{[](const auto& p) -> double { return -std::sqrt(2) * p[0]; }};

    // Apply feedforward: Measurement result of mode 0 is forwarded to d_x.
    auto ff_x = bosim::FeedForward<double>{{2}, {4, 0}, ff_func_x};
    circuit.AddFF(ff_x);

    // Apply feedforward: Measurement result of mode 1 is forwarded to d_p.
    auto ff_p = bosim::FeedForward<double>{{3}, {4, 1}, ff_func_p};
    circuit.AddFF(ff_p);

    // Define initial quantum state.
    auto state = bosim::State<double>{
        {bosim::SingleModeMultiPeak<double>::GenerateSqueezed(0, 0, std::complex<double>{x, p}),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel, Pi / 2,
                                                              std::complex<double>{0, 0}),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel, 0,
                                                              std::complex<double>{0, 0})}};
    return {std::move(circuit), std::move(state)};
}
void RunTeleportation(const bosim::SimulateSettings& settings, const double x, const double p) {
    std::cout << "Simulating teleportation of state (x, p) = (" << x << ", " << p << ")..."
              << std::endl;
    auto [circuit, state] = CreateTeleportationCircuit(x, p);

    const auto result = bosim::Simulate<double>(settings, circuit, state);
    const auto& state_after_simulation = result.result[0].state.value();
    const auto& mean = state_after_simulation.GetMean(0);
    const auto teleported_x = mean(2).real();
    const auto teleported_p = mean(5).real();

    const auto error_x = std::abs(x - teleported_x);
    const auto error_p = std::abs(p - teleported_p);

    if (error_x > Threshold || error_p > Threshold) {
        std::cerr << Yellow << "Teleportation failed" << Reset << std::endl;
        std::cout << Yellow << "  Expected: (x, p) = (" << x << ", " << p << ")" << Reset
                  << std::endl;
        std::cout << Yellow << "  Got:      (x, p) = (" << teleported_x << ", " << teleported_p
                  << ")" << Reset << std::endl;
    } else {
        std::cout << Green << "Teleportation successful" << Reset << std::endl;
    }
}
int main() {
    constexpr auto NShots = std::size_t{1};
    const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::CPUSingleThread,
                                                  bosim::StateSaveMethod::All};

    std::cout << "Squeezing level: " << SqueezingLevel << " dB" << std::endl;
    std::cout << "Error threshold: " << Threshold << std::endl;
    for (const auto& x : {1.5, -20.0, 0.01}) {
        for (const auto& p : {1.5, -20.0, 0.01}) { RunTeleportation(settings, x, p); }
    }
}
