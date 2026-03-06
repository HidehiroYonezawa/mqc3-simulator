#include "bosim/circuit.h"

#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"
#include "bosim/simulator.h"

#include "utility.h"

TEST(Circuit, StructSize) {
    static_assert(sizeof(bosim::OperationVariant<double>) == 16);
    static_assert(sizeof(bosim::OperationVariant<float>) == 16);
}

TEST(Circuit, HomodyneSingleModePostSelection) {
    auto state = bosim::State<double>::GenerateCat(std::complex<double>{0, 2.0}, 0);
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0, 1.5);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0, 1.5);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(
        0, std::numbers::pi_v<double> / 3, 1.8);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(
        0, std::numbers::pi_v<double> / 4, 1.8);
    const auto settings =
        bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread, bosim::StateSaveMethod::None};
    const auto measured_values =
        bosim::Simulate(settings, circuit, state).result[0].measured_values;
    EXPECT_DOUBLE_EQ(measured_values.at(0), 1.5);
    EXPECT_DOUBLE_EQ(measured_values.at(1), 1.5);
    EXPECT_DOUBLE_EQ(measured_values.at(2), 1.8);
    EXPECT_DOUBLE_EQ(measured_values.at(3), 1.8);
}

TEST(Circuit, Teleportation) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    constexpr double SqueezingLevel = 10.0;

    constexpr auto Pi = std::numbers::pi_v<double>;
    constexpr auto Phi = std::numbers::pi_v<double> / 3;
    const auto t0 = (Pi + Phi) / 2;
    const auto t1 = Phi / 2;

    constexpr std::int32_t SampleSize = 10000;

    auto sample_x = std::vector<double>{};
    auto sample_p = std::vector<double>{};
    sample_x.reserve(SampleSize);
    sample_p.reserve(SampleSize);
    const double coeff = -std::sqrt(2) / std::sin(t1 - t0);
    auto simulator = bosim::Simulator<double>{static_cast<std::size_t>(SampleSize)};
    for (std::int32_t i = 0; i < SampleSize; ++i) {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::util::BeamSplitter50<double>>(1, 2);
        circuit.EmplaceOperation<bosim::operation::util::BeamSplitter50<double>>(0, 1);
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, t0);
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(1, t1);
        circuit.EmplaceOperation<bosim::operation::Displacement<double>>(2, 0, 0);
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(2, Pi / 2 - Phi);

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

        auto state = bosim::State<double>{
            {bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel),
             bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel, Pi / 2),
             bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel)}};
        state.SetSeed(static_cast<std::uint64_t>(i));

        auto state_copy = state;
        state.SetShot(static_cast<std::size_t>(i));
        state_copy.SetShot(static_cast<std::size_t>(i));

        simulator.Execute(state, circuit);
        sample_x.push_back(simulator.GetMeasuredVal(static_cast<std::size_t>(i), 5));

        auto& ops = circuit.GetMutOperations();
        ops.pop_back();
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(2, -Phi);
        simulator.Execute(state_copy, circuit);
        sample_p.push_back(simulator.GetMeasuredVal(static_cast<std::size_t>(i), 5));
    }

    const auto json_file =
        fmt::format("{}_{}_result.json", test_info->test_suite_name(), test_info->name());
    auto config = std::map<std::string, nlohmann::json>{};
    config["squeezing_level"] = SqueezingLevel;
    config["phi"] = Phi;
    config["sample_x"] = sample_x;
    config["sample_p"] = sample_p;

    AddToJson(json_file, config, true);
    TestInPython("circuit", "test_teleportation", json_file);
}
