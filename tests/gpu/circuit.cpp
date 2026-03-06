#include "bosim/circuit.h"

#include <gtest/gtest.h>

#include <chrono>

#include "bosim/exception.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"

#include "base/utility.h"

// Input the initial state into a circuit with no operations, run the simulation, and compare
// the output state with the initial state.
TEST(GPU, CompareState) {
    const auto state_vac = bosim::State<double>::GenerateVacuum();
    const auto state_cat = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 0}, 0);

    auto circuit = bosim::Circuit<double>{};
    const auto settings =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly};
    const auto state_vac_gpu =
        bosim::Simulate(settings, circuit, state_vac).result[0].state.value();
    const auto state_cat_gpu =
        bosim::Simulate(settings, circuit, state_cat).result[0].state.value();

    EXPECT_TRUE(CompareState(state_vac, state_vac_gpu));
    EXPECT_TRUE(CompareState(state_cat, state_cat_gpu));
    EXPECT_FALSE(CompareState(state_vac, state_cat_gpu));
    EXPECT_FALSE(CompareState(state_cat, state_vac_gpu));
}

// Execute a circuit and validate the output states with entangled and unentangled state.
TEST(GPU, StateGPUConstructor) {
    constexpr double Theta = std::numbers::pi / 3;

    // Circuit
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
        0, 1, std::abs(std::cos(Theta)), Theta);
    circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
        0, 2, std::abs(std::cos(Theta)), Theta);
    circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
        1, 2, std::abs(std::cos(Theta)), Theta);

    // State
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});
    auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({4, 2}, {0, 3, 0, 1},
                                                                             {1, 0, 0, 0});
    auto mode0 = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};
    auto mode1 = bosim::SingleModeMultiPeak<double>{{peak0, peak1, peak2}};
    auto mode2 = bosim::SingleModeMultiPeak<double>{{peak1, peak2}};
    auto modes = std::vector{mode0, mode1, mode2};
    auto state = bosim::State<double>(modes);

    // Settings
    const auto settings_cpu = bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread,
                                                      bosim::StateSaveMethod::FirstOnly};
    const auto settings_gpu =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly};

    const auto result_state_cpu =
        bosim::Simulate(settings_cpu, circuit, state).result[0].state.value();  // ground truth
    const auto result_state_gpu_from_state =
        bosim::Simulate(settings_gpu, circuit, state).result[0].state.value();
    const auto result_state_gpu_from_modes =
        bosim::Simulate(settings_gpu, circuit, modes).result[0].state.value();
    EXPECT_TRUE(CompareState(result_state_cpu, result_state_gpu_from_state));
    EXPECT_TRUE(CompareState(result_state_cpu, result_state_gpu_from_modes));
}

// Execute a circuit with a single displacement on both CPU and GPU, and compare the output
// states.
TEST(GPU, Displacement) {
    constexpr double Dx = 2;
    constexpr double Dp = 3;

    // Circuit
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::Displacement<double>>(0, Dx, Dp);

    // State
    auto state = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 1.0}, 0);

    // Simulate
    const auto settings_cpu = bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread,
                                                      bosim::StateSaveMethod::FirstOnly};
    const auto settings_gpu =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly};
    const auto result_state_cpu =
        bosim::Simulate(settings_cpu, circuit, state).result[0].state.value();
    const auto result_state_gpu =
        bosim::Simulate(settings_gpu, circuit, state).result[0].state.value();

    EXPECT_TRUE(CompareState(result_state_cpu, result_state_gpu));
}

// Execute a circuit with `ReplaceBySqueezedState` operations on both CPU and GPU, and
// compare the output states.
TEST(GPU, ReplaceBySqueezedState) {
    // Circuit
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::util::ReplaceBySqueezedState<double>>(
        0, 10.0, 0.25 * std::numbers::pi);
    circuit.EmplaceOperation<bosim::operation::util::ReplaceBySqueezedState<double>>(
        1, 20.0, 0.5 * std::numbers::pi);

    // State
    const auto single_mode_state =
        bosim::SingleModeMultiPeak<double>::GenerateCat(std::complex<double>{2.0, 1.0}, 0);
    auto state = bosim::State<double>{{single_mode_state, single_mode_state}};

    // Simulate
    const auto settings_cpu = bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread,
                                                      bosim::StateSaveMethod::FirstOnly};
    const auto settings_gpu =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly};
    const auto result_state_cpu =
        bosim::Simulate(settings_cpu, circuit, state).result[0].state.value();
    const auto result_state_gpu =
        bosim::Simulate(settings_gpu, circuit, state).result[0].state.value();

    EXPECT_TRUE(CompareState(result_state_cpu, result_state_gpu));
}

// Execute a circuit with a single phase rotation on both CPU and GPU, and compare the output
// states.
TEST(GPU, PhaseRotation) {
    constexpr double Phi = std::numbers::pi_v<double> / 6;

    // Circuit
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, Phi);

    // State
    auto state = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 1.0}, 0);

    // Simulate
    const auto settings_cpu = bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread,
                                                      bosim::StateSaveMethod::FirstOnly};
    const auto settings_gpu =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly};
    const auto result_state_cpu =
        bosim::Simulate(settings_cpu, circuit, state).result[0].state.value();
    const auto result_state_gpu =
        bosim::Simulate(settings_gpu, circuit, state).result[0].state.value();

    EXPECT_TRUE(CompareState(result_state_cpu, result_state_gpu));
}

// Execute a circuit applying beam splitters between modes (0,1), (1,2), and (1,2) on both CPU
// and GPU, and compare the output states. By entangling the modes, the calculation of not only
// the block-diagonal elements but also the block off-diagonal elements of the covariance matrix
// is validated.
TEST(GPU, BeamSplitter) {
    constexpr double Theta = std::numbers::pi / 3;

    // Circuit
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
        0, 1, std::abs(std::cos(Theta)), Theta);
    circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
        0, 2, std::abs(std::cos(Theta)), Theta);
    circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
        1, 2, std::abs(std::cos(Theta)), Theta);

    // State
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});
    auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({4, 2}, {0, 3, 0, 1},
                                                                             {1, 0, 0, 0});
    auto mode0 = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};
    auto mode1 = bosim::SingleModeMultiPeak<double>{{peak0, peak1, peak2}};
    auto mode2 = bosim::SingleModeMultiPeak<double>{{peak1, peak2}};
    auto state = bosim::State<double>{{mode0, mode1, mode2}};

    // Simulate
    const auto settings_cpu = bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread,
                                                      bosim::StateSaveMethod::FirstOnly};
    const auto settings_gpu =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly};
    const auto result_state_cpu =
        bosim::Simulate(settings_cpu, circuit, state).result[0].state.value();
    const auto result_state_gpu =
        bosim::Simulate(settings_gpu, circuit, state).result[0].state.value();

    EXPECT_TRUE(CompareState(result_state_cpu, result_state_gpu));
}

// Check whether the result of the measurement operation, given a specified measured value,
// trivially matches the specified value.
TEST(GPU, HomodyneSingleModeXPostSelection) {
    auto state = bosim::State<double>::GenerateCat(std::complex<double>{0, 2.0}, 0);
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0, 1.5);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0, 1.5);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0, 1.8);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0, 1.8);
    const auto settings =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::None};
    const auto measured_values =
        bosim::Simulate(settings, circuit, state).result[0].measured_values;
    EXPECT_DOUBLE_EQ(measured_values.at(0), 1.5);
    EXPECT_DOUBLE_EQ(measured_values.at(1), 1.5);
    EXPECT_DOUBLE_EQ(measured_values.at(2), 1.8);
    EXPECT_DOUBLE_EQ(measured_values.at(3), 1.8);
}

/*
Create JSON data in the following format, read it in Python, and run tests.
{
    "state": {"squeezing_level": float, "phi": float, "theta": float},
    "n_modes": int,
    "n_peaks": int,
    "peaks": {
        "coeff": {"real": float, "imag": float},
        "mean": [{"real": float, "imag": float}...],
        "cov": [float...]
    },
    "select": float,
    "measured_vals": [float...]
    ""
}
*/
TEST(GPU, HomodyneSingleMode1ModeSqueezed) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    // Generate a single squeezed state
    auto config = std::map<std::string, nlohmann::json>{};
    constexpr double SqueezingLevel = 10.0;
    constexpr auto Phi = std::numbers::pi_v<double> / 3;
    constexpr auto Theta = std::numbers::pi_v<double> / 6;
    config["state"]["squeezing_level"] = SqueezingLevel;
    config["state"]["phi"] = Phi;
    config["state"]["theta"] = Theta;
    auto state =
        bosim::State<double>::GenerateSqueezed(SqueezingLevel, Phi, std::complex<double>{0, 0});

    // Test of a single squeezed state subject to homodyne measurement:
    // 1. Post-selection
    // Copy the squeezed state, perform homodyne measurement post-selection at
    // a specific value, and compare the output state with the result from Strawberry Fields.
    // 2. Random measurement
    // Repeat copying the squeezed state and performing homodyne measurement
    // multiple times, then statistically compare the measured values with the results from
    // Strawberry Fields.

    const auto json_file =
        fmt::format("{}_{}_after.json", test_info->test_suite_name(), test_info->name());

    // Post-selection
    {
        auto circuit = bosim::Circuit<double>{};
        config["select"] = 0.3;
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, Theta,
                                                                               config["select"]);
        const auto settings = bosim::SimulateSettings{1, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::FirstOnly};
        const auto result_state = bosim::Simulate(settings, circuit, state).result[0].state.value();
        result_state.DumpToJson(json_file);
    }

    // Random measurement
    {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, Theta);
        constexpr std::size_t NShots = 10000;
        const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::None};
        const auto result = bosim::Simulate(settings, circuit, state).result;
        auto measured_vals = std::vector<double>(NShots);
        for (std::size_t i = 0; i < NShots; ++i) {
            measured_vals[i] = result[i].measured_values.at(0);
        }
        config["measured_vals"] = measured_vals;
        AddToJson(json_file, config);
    }

    TestInPython("operation", "test_1squeezed_homodyne", json_file);
}

/*
Create JSON data in the following format, read it in Python, and run tests.
{
    "state": {"real": float, "imag": float},
    "n_modes": int,
    "n_peaks": int,
    "peaks": {
        "coeff": {"real": float, "imag": float},
        "mean": [{"real": float, "imag": float}...],
        "cov": [float...]
    },
    "select": float,
    "measured_vals": [float...]
    ""
}
*/
TEST(GPU, HomodyneSingleModeX1ModeCat) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    // Generate a single cat state
    auto config = std::map<std::string, nlohmann::json>{};
    config["state"]["real"] = 2.0;
    config["state"]["imag"] = 0.0;
    const auto& config_state =
        std::complex<double>{config["state"]["real"], config["state"]["imag"]};
    auto cat_state = bosim::State<double>::GenerateCat(config_state, 0);

    // Test of a single cat state subject to X homodyne measurement:
    // 1. Post-selection
    // Copy the cat state, perform x-basis homodyne measurement post-selection at
    // a specific value, and compare the output state with the result from Strawberry Fields.
    // 2. Random measurement
    // Repeat copying the cat state and performing x-basis homodyne measurement
    // multiple times, then statistically compare the measured values with the results from
    // Strawberry Fields.

    const auto json_file =
        fmt::format("{}_{}_after.json", test_info->test_suite_name(), test_info->name());

    // Post-selection
    {
        auto circuit = bosim::Circuit<double>{};
        config["select"] = 0.3;
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0,
                                                                                config["select"]);
        const auto settings = bosim::SimulateSettings{1, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::FirstOnly};
        const auto result_state =
            bosim::Simulate(settings, circuit, cat_state).result[0].state.value();
        result_state.DumpToJson(json_file);
    }

    // Random measurement
    {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(0);
        constexpr std::size_t NShots = 1000;
        const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::None};
        const auto result = bosim::Simulate(settings, circuit, cat_state).result;

        auto measured_vals = std::vector<double>(NShots);
        for (std::size_t i = 0; i < NShots; ++i) {
            measured_vals[i] = result[i].measured_values.at(0);
        }
        config["measured_vals"] = measured_vals;
        AddToJson(json_file, config);
    }

    TestInPython("operation", "test_1cat_homodyne", json_file);
}

// Test of a state subject to X homodyne measurement:
// 1. Post-selection
// Copy the state, perform x-basis homodyne measurement post-selection at a specific value, and
// compare the output state with the result from Strawberry Fields.
// 2. Random measurement
// Repeat copying the state and performing x-basis homodyne measurement multiple times, then
// statistically compare the measured values with the results from Strawberry Fields.
void HomodyneSingleModeX2Mode(std::map<std::string, nlohmann::json>& config,
                              const bosim::State<double>& state, std::string test_suite_name,
                              std::string name) {
    constexpr std::size_t MeasuredMode = 0;
    const auto json_file = fmt::format("{}_{}_two_cats_bs_homodyne.json", test_suite_name, name);
    constexpr auto Pi = std::numbers::pi_v<double>;
    config["bs"]["theta"] = -Pi / 3;
    config["bs"]["phi"] = Pi / 4;
    const double theta = config["bs"]["theta"];
    const double phi = config["bs"]["phi"];

    // Post-selection
    {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, phi - Pi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 2);
        circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
            0, 1, std::abs(std::cos(theta)), theta);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, Pi - theta - phi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, Pi / 2 - theta);

        auto select = nlohmann::json{};
        select[MeasuredMode] = 0.3;
        config["select"] = select;
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(
            MeasuredMode, config["select"][MeasuredMode]);
        const auto settings = bosim::SimulateSettings{1, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::FirstOnly};
        const auto result_state = bosim::Simulate(settings, circuit, state).result[0].state.value();
        result_state.DumpToJson(json_file);
    }

    // Random measurement
    {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, phi - Pi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 2);
        circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
            0, 1, std::abs(std::cos(theta)), theta);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, Pi - theta - phi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, Pi / 2 - theta);

        circuit.EmplaceOperation<bosim::operation::HomodyneSingleModeX<double>>(MeasuredMode);
        constexpr std::size_t NShots = 1000;
        const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::None};
        const auto result = bosim::Simulate(settings, circuit, state).result;

        auto measured_vals = std::vector<double>(NShots);
        for (std::size_t i = 0; i < NShots; ++i) {
            measured_vals[i] = result[i].measured_values.at(5);
        }
        config["measured_vals"] = measured_vals;
        AddToJson(json_file, config);
        AddWignerToJson<double>(json_file, state, {0, 1});
    }
    TestInPython("operation", "test_two_cats_bs_homodyne", json_file);
}

// Test of a state subject to homodyne measurement:
// 1. Post-selection
// Copy the state, perform homodyne measurement post-selection at a specific value, and compare
// the output state with the result from Strawberry Fields.
// 2. Random measurement
// Repeat copying the state and performing homodyne measurement multiple times, then
// statistically compare the measured values with the results from Strawberry Fields.
void HomodyneSingleMode2Mode(std::map<std::string, nlohmann::json>& config,
                             const bosim::State<double>& state, std::string test_suite_name,
                             std::string name) {
    constexpr std::size_t MeasuredMode = 0;
    const auto json_file =
        fmt::format("{}_{}_two_cats_bs_homodyne_theta.json", test_suite_name, name);
    constexpr auto Pi = std::numbers::pi_v<double>;
    config["bs"]["theta"] = -Pi / 3;
    config["bs"]["phi"] = Pi / 4;
    const double theta = config["bs"]["theta"];
    const double phi = config["bs"]["phi"];
    config["homodyne_angle_theta"] = std::numbers::pi_v<double> / 6;
    auto homodyne =
        bosim::operation::HomodyneSingleMode<double>{MeasuredMode, config["homodyne_angle_theta"]};

    // Post-selection
    {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, phi - Pi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 2);
        circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
            0, 1, std::abs(std::cos(theta)), theta);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, Pi - theta - phi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, Pi / 2 - theta);

        auto select = nlohmann::json{};
        select[MeasuredMode] = 0.3;
        config["select"] = select;
        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(
            MeasuredMode, config["homodyne_angle_theta"], config["select"][MeasuredMode]);
        const auto settings = bosim::SimulateSettings{1, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::FirstOnly};
        const auto result_state = bosim::Simulate(settings, circuit, state).result[0].state.value();
        result_state.DumpToJson(json_file);
    }

    // Random measurement
    {
        auto circuit = bosim::Circuit<double>{};
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, phi - Pi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 2);
        circuit.EmplaceOperation<bosim::operation::BeamSplitter<double>>(
            0, 1, std::abs(std::cos(theta)), theta);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, Pi - theta - phi);
        circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, Pi / 2 - theta);

        circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(
            MeasuredMode, config["homodyne_angle_theta"]);
        constexpr std::size_t NShots = 1000;
        const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::SingleGPU,
                                                      bosim::StateSaveMethod::None};
        const auto result = bosim::Simulate(settings, circuit, state).result;

        auto measured_vals = std::vector<double>(NShots);
        for (std::size_t i = 0; i < NShots; ++i) {
            measured_vals[i] = result[i].measured_values.at(5);
        }
        config["measured_vals"] = measured_vals;
        AddToJson(json_file, config);
        AddWignerToJson<double>(json_file, state, {0, 1});
    }
    TestInPython("operation", "test_two_cats_bs_homodyne_theta", json_file);
}

/*
Create JSON data in the following format, read it in Python, and run tests.
{
    "bs": {"theta": float, "phi": float},
    "state0": {"real": float, "imag": float},
    "state1": {"real": float, "imag": float},
    "n_modes": int,
    "n_peaks": int,
    "peaks": {
    "coeff": {"real": float, "imag": float},
    "mean": [{"real": float, "imag": float}...],
    "cov": [float...]
    },
    "wigner": {
        "start": float,
        "stop": float,
        "num": int,
        "vals": {"<mode (int)>": [float...] ...}
    },
    "select": {0: float},
    "measured_vals": [float...]
""
}
*/
TEST(GPU, HomodyneSingleMode2Mode) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    // Generate two cat states
    auto config = std::map<std::string, nlohmann::json>{};
    config["state0"]["real"] = 2.0;
    config["state0"]["imag"] = 1.0;
    config["state1"]["real"] = std::sqrt(2.0);
    config["state1"]["imag"] = std::sqrt(2.0);
    const auto& config_state0 =
        std::complex<double>{config["state0"]["real"], config["state0"]["imag"]};
    const auto& config_state1 =
        std::complex<double>{config["state1"]["real"], config["state1"]["imag"]};
    auto cat_state0 = bosim::SingleModeMultiPeak<double>::GenerateCat(config_state0, 0);
    auto cat_state1 = bosim::SingleModeMultiPeak<double>::GenerateCat(config_state1, 0);
    auto state = bosim::State<double>{{cat_state0, cat_state1}};

    // Test measurement
    HomodyneSingleModeX2Mode(config, state, test_info->test_suite_name(), test_info->name());
    HomodyneSingleMode2Mode(config, state, test_info->test_suite_name(), test_info->name());
}

TEST(GPU, SelectDevice) {
    const auto state_vac = bosim::State<double>::GenerateVacuum();
    const auto state_cat = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 0}, 0);

    auto circuit = bosim::Circuit<double>{};
    const auto settings = bosim::SimulateSettings{
        10,        bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly, 1, std::nullopt,
        {10, 2, 0}};
    const auto state_vac_gpu =
        bosim::Simulate(settings, circuit, state_vac).result[0].state.value();
    const auto state_cat_gpu =
        bosim::Simulate(settings, circuit, state_cat).result[0].state.value();

    EXPECT_TRUE(CompareState(state_vac, state_vac_gpu));
    EXPECT_TRUE(CompareState(state_cat, state_cat_gpu));
    EXPECT_FALSE(CompareState(state_vac, state_cat_gpu));
    EXPECT_FALSE(CompareState(state_cat, state_vac_gpu));
}

TEST(MultiGPU, SelectDevice) {
    const auto state_vac = bosim::State<double>::GenerateVacuum();
    const auto state_cat = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 0}, 0);

    auto circuit = bosim::Circuit<double>{};
    const auto settings = bosim::SimulateSettings{
        10,          bosim::Backend::MultiGPU, bosim::StateSaveMethod::FirstOnly, 1, std::nullopt,
        {2, 3, 0, 1}};
    const auto state_vac_gpu =
        bosim::Simulate(settings, circuit, state_vac).result[0].state.value();
    const auto state_cat_gpu =
        bosim::Simulate(settings, circuit, state_cat).result[0].state.value();

    EXPECT_TRUE(CompareState(state_vac, state_vac_gpu));
    EXPECT_TRUE(CompareState(state_cat, state_cat_gpu));
    EXPECT_FALSE(CompareState(state_vac, state_cat_gpu));
    EXPECT_FALSE(CompareState(state_cat, state_vac_gpu));
}

TEST(GPU, Timeout) {
    const auto state_vac = bosim::State<double>::GenerateVacuum();
    const auto state_cat = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 0}, 0);

    auto circuit = bosim::Circuit<double>{};
    const auto settings = bosim::SimulateSettings{
        10,        bosim::Backend::SingleGPU, bosim::StateSaveMethod::FirstOnly,
        1,         std::chrono::seconds(-1),  // Set timeout -1
        {10, 2, 0}};

    EXPECT_THROW(bosim::Simulate(settings, circuit, state_vac).result[0].state.value(),
                 bosim::SimulationError);
}

TEST(MultiGPU, Timeout) {
    const auto state_vac = bosim::State<double>::GenerateVacuum();
    const auto state_cat = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 0}, 0);

    auto circuit = bosim::Circuit<double>{};
    const auto settings = bosim::SimulateSettings{
        10,          bosim::Backend::MultiGPU, bosim::StateSaveMethod::FirstOnly,
        1,           std::chrono::seconds(-1),  // Set timeout -1
        {2, 3, 0, 1}};

    EXPECT_THROW(bosim::Simulate(settings, circuit, state_vac).result[0].state.value(),
                 bosim::SimulationError);
}
