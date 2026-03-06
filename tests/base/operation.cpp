#include "bosim/operation.h"

#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <filesystem>  // NOLINT
#include <fstream>

#include "bosim/simulate/simulate.h"

#include "utility.h"

// Comparison between the result of displacing the vacuum state and the ideal coherent state
TEST(Operation, Displacement) {
    constexpr double Dx = 2;
    constexpr double Dp = 3;
    auto state = bosim::State<double>::GenerateVacuum();
    auto displacement = bosim::operation::Displacement<double>{0, Dx, Dp};
    displacement(state);
    const auto target_state = bosim::State<double>::GenerateCoherent(std::complex<double>{Dx, Dp});
    EXPECT_TRUE(CompareState(state, target_state));
}

// Comparison between the result of phase-rotating a coherent state and the ideal coherent state
TEST(Operation, PhaseRotation) {
    constexpr double Phi = std::numbers::pi_v<double> / 6;
    auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{2, 0});
    auto phase_rotation = bosim::operation::PhaseRotation<double>{0, Phi};
    phase_rotation(state);
    const auto target_state =
        bosim::State<double>::GenerateCoherent(std::complex<double>{std::sqrt(3), 1.0});
    EXPECT_TRUE(CompareState(state, target_state));
}

// Comparison between the result of squeezing the vacuum state and then rotating by -pi/2, and the
// ideal squeezed state
TEST(Operation, Squeezing) {
    constexpr double Phi = -std::numbers::pi_v<double> / 2;
    constexpr double Theta = std::numbers::pi_v<double> / 3;
    auto state = bosim::State<double>::GenerateVacuum();
    auto squeezing = bosim::operation::Squeezing<double>{0, Theta};
    squeezing(state);

    const double cot = 1 / std::tan(Theta);
    const double squeezing_level = 20 * std::log10(cot);
    const auto target_state =
        bosim::State<double>::GenerateSqueezed(squeezing_level, Phi, std::complex<double>{0, 0});

    EXPECT_TRUE(CompareState(state, target_state));
}

// Comparison between the result of applying the Squeezing45 operation to a squeezed state with a
// squeezing angle of -pi/4 and the ideal squeezed state
TEST(Operation, Squeezing45) {
    const double squeezing_level_initial = 20 * std::log10(3);
    auto state = bosim::State<double>::GenerateSqueezed(
        squeezing_level_initial, -std::numbers::pi_v<double> / 4, std::complex<double>{0, 0});
    constexpr double Theta = std::numbers::pi_v<double> / 3;
    auto squeezing45 = bosim::operation::Squeezing45<double>{0, Theta};
    squeezing45(state);

    const double cot = 1 / std::tan(Theta);
    const double squeezing_level = 20 * std::log10(cot);
    constexpr double Phi = -std::numbers::pi_v<double> / 4;
    const auto target_state = bosim::State<double>::GenerateSqueezed(
        squeezing_level_initial + squeezing_level, Phi, std::complex<double>{0, 0});

    EXPECT_TRUE(CompareState(state, target_state));
}

// Comparison between the result of applying the ShearXInvariant operation to a coherent state and
// the ideal state
TEST(Operation, ShearXInvariant) {
    constexpr double Kappa = 1.0;
    auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto shear = bosim::operation::ShearXInvariant<double>{0, Kappa};
    shear(state);

    auto expected_mean = bosim::Vector2C<double>{};
    expected_mean << std::complex<double>{1, 0}, std::complex<double>{3, 0};
    auto expected_covariance = bosim::Matrix2D<double>{};
    expected_covariance << 1, 2, 2, 5;
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 2; ++j) {
        EXPECT_DOUBLE_EQ(state.GetMean(0)(j).real(), expected_mean(j).real());
        EXPECT_DOUBLE_EQ(state.GetMean(0)(j).imag(), expected_mean(j).imag());
        for (long k = 0; k < 2; ++k) {
            EXPECT_DOUBLE_EQ(state.GetCov(0)(j, k), expected_covariance(j, k));
        }
    }
}

// Comparison between the result of applying the ShearPInvariant operation to a coherent state and
// the ideal state
TEST(Operation, ShearPInvariant) {
    constexpr double Eta = 1.0;
    auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto shear = bosim::operation::ShearPInvariant<double>{0, Eta};
    shear(state);

    auto expected_mean = bosim::Vector2C<double>{};
    expected_mean << std::complex<double>{3, 0}, std::complex<double>{1, 0};
    auto expected_covariance = bosim::Matrix2D<double>{};
    expected_covariance << 5, 2, 2, 1;
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 2; ++j) {
        EXPECT_DOUBLE_EQ(state.GetMean(0)(j).real(), expected_mean(j).real());
        EXPECT_DOUBLE_EQ(state.GetMean(0)(j).imag(), expected_mean(j).imag());
        for (long k = 0; k < 2; ++k) {
            EXPECT_DOUBLE_EQ(state.GetCov(0)(j, k), expected_covariance(j, k));
        }
    }
}

// Comparison between the result of applying the Arbitrary operation to a coherent state and
// the ideal state
TEST(Operation, Arbitrary) {
    constexpr double Alpha = std::numbers::pi_v<double> / 2;
    constexpr double Beta = std::numbers::pi_v<double> / 4;
    constexpr double Lambda = -1.0;
    auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto arb = bosim::operation::Arbitrary<double>{0, Alpha, Beta, Lambda};
    arb(state);

    auto expected_mean = bosim::Vector2C<double>{};
    const auto e = std::exp(1.0);
    expected_mean << std::complex<double>{-std::sqrt(2) / e, 0}, std::complex<double>{0, 0};
    auto expected_covariance = bosim::Matrix2D<double>{};
    expected_covariance << 1 / (e * e), 0, 0, e * e;
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 2; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 2; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
}

// Comparison between the result of applying the BeamSplitter operation to coherent states and the
// ideal state
TEST(Operation, BeamSplitter) {
    constexpr double SqrtR = 0.5;
    constexpr double ThetaRel = 2 * std::numbers::pi_v<double> / 3;
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    auto beamsplitter = bosim::operation::BeamSplitter<double>{0, 1, SqrtR, ThetaRel};
    beamsplitter(state);

    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{-1, 0}, std::complex<double>{1, 0},
        std::complex<double>{-1, 0}, std::complex<double>{1, 0};
    auto expected_covariance = bosim::MatrixD<double>{4, 4};
    expected_covariance.setIdentity();
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 4; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 4; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
}

// Comparison between the result of applying the ControlledZ operation to coherent states and the
// ideal state
TEST(Operation, ControlledZ) {
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    constexpr double G = 2;
    auto controlled_z = bosim::operation::ControlledZ<double>{0, 1, G};
    controlled_z(state);

    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{1, 0}, std::complex<double>{-1, 0},
        std::complex<double>{1 - G, 0}, std::complex<double>{G - 1, 0};
    auto expected_covariance = bosim::MatrixD<double>{4, 4};
    expected_covariance << 1, 0, 0, G, 0, 1, G, 0, 0, G, 1 + G * G, 0, G, 0, 0, 1 + G * G;
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 4; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 4; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
}

// Comparison between the result of applying the TwoModeShear operation to coherent states and the
// ideal state
TEST(Operation, TwoModeShear) {
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    constexpr double A = 2;
    constexpr double B = -3;
    auto two_mode_shear = bosim::operation::TwoModeShear<double>{0, 1, A, B};
    two_mode_shear(state);

    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{1, 0}, std::complex<double>{-1, 0},
        std::complex<double>{2 * A - B + 1, 0}, std::complex<double>{B - 2 * A - 1, 0};
    auto expected_covariance = bosim::MatrixD<double>{4, 4};
    constexpr auto Block11Diag = 4 * A * A + B * B + 1;
    expected_covariance << 1, 0, 2 * A, B, 0, 1, B, 2 * A, 2 * A, B, Block11Diag, 4 * A * B, B,
        2 * A, 4 * A * B, Block11Diag;
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 4; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 4; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
}

// Comparison between the result of applying the Manual operation to coherent states and the ideal
// state
TEST(Operation, Manual) {
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    constexpr double ThetaA = 0.5;
    constexpr double ThetaB = 0.1;
    constexpr double ThetaC = -0.3;
    constexpr double ThetaD = 1.6;
    auto manual = bosim::operation::Manual<double>{0, 1, ThetaA, ThetaB, ThetaC, ThetaD};
    manual(state);

    const double denom_ba = -(std::sin(ThetaA - ThetaB));
    const double denom_dc = -(std::sin(ThetaC - ThetaD));
    const auto sc_ba =
        (std::sin(ThetaA) * std::cos(ThetaB) + std::cos(ThetaA) * std::sin(ThetaB)) / denom_ba;
    const auto cc_ba = (2 * std::cos(ThetaA) * std::cos(ThetaB)) / denom_ba;
    const auto ss_ba = (2 * std::sin(ThetaA) * std::sin(ThetaB)) / denom_ba;
    const auto sc_dc =
        (std::sin(ThetaC) * std::cos(ThetaD) + std::cos(ThetaC) * std::sin(ThetaD)) / denom_dc;
    const auto cc_dc = (2 * std::cos(ThetaC) * std::cos(ThetaD)) / denom_dc;
    const auto ss_dc = (2 * std::sin(ThetaC) * std::sin(ThetaD)) / denom_dc;
    const auto sc_p = (sc_ba + sc_dc) / 2;
    const auto sc_m = (-sc_ba + sc_dc) / 2;
    const auto cc_p = (cc_ba + cc_dc) / 2;
    const auto cc_m = (-cc_ba + cc_dc) / 2;
    const auto ss_p = (ss_ba + ss_dc) / 2;
    const auto ss_m = (-ss_ba + ss_dc) / 2;

    auto op_matrix = bosim::MatrixD<double>{4, 4};
    op_matrix << sc_p, sc_m, cc_p, cc_m, sc_m, sc_p, cc_m, cc_p, ss_p, ss_m, sc_p, sc_m, ss_m, ss_p,
        sc_m, sc_p;
    for (long j = 0; j < 4; ++j) {
        for (long k = 0; k < 4; ++k) {
            EXPECT_DOUBLE_EQ(op_matrix(j, k), manual.GetOperationMatrix()(j, k));
        }
    }

    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{sc_ba + cc_ba, 0},
        std::complex<double>{-sc_ba - cc_ba, 0}, std::complex<double>{sc_ba + ss_ba, 0},
        std::complex<double>{-sc_ba - ss_ba, 0};
    auto expected_block = [](const double p0, const double m0, const double p1, const double m1,
                             const double p2, const double m2, const double p3, const double m3) {
        auto block = bosim::Matrix2D<double>{};
        const auto diag = p0 * p1 + m0 * m1 + p2 * p3 + m2 * m3;
        const auto off_diag = p0 * m1 + m0 * p1 + p2 * m3 + m2 * p3;
        block << diag, off_diag, off_diag, diag;
        return block;
    };
    auto expected_covariance = bosim::MatrixD<double>{4, 4};
    expected_covariance.block(0, 0, 2, 2) =
        expected_block(sc_p, sc_m, sc_p, sc_m, cc_p, cc_m, cc_p, cc_m);
    expected_covariance.block(0, 2, 2, 2) =
        expected_block(sc_p, sc_m, ss_p, ss_m, cc_p, cc_m, sc_p, sc_m);
    expected_covariance.block(2, 0, 2, 2) =
        expected_block(ss_p, ss_m, sc_p, sc_m, sc_p, sc_m, cc_p, cc_m);
    expected_covariance.block(2, 2, 2, 2) =
        expected_block(ss_p, ss_m, ss_p, ss_m, sc_p, sc_m, sc_p, sc_m);
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 4; ++j) {
        EXPECT_DOUBLE_EQ(state.GetMean(0)(j).real(), expected_mean(j).real());
        EXPECT_DOUBLE_EQ(state.GetMean(0)(j).imag(), expected_mean(j).imag());
        for (long k = 0; k < 4; ++k) {
            EXPECT_DOUBLE_EQ(state.GetCov(0)(j, k), expected_covariance(j, k));
        }
    }
}

TEST(Operation, HomodyneSingleModeXSinglePeak) {
    //                 ⎡  1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 0i, v = ⎢         ⎥, m = ⎢      ⎥
    //                 ⎣ -1 + 0i ⎦      ⎣ 2  4 ⎦
    const auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays(
        {1, 0}, {1, 0, 2, 0}, {1, 2, 2, 4});
    const auto mode0 = bosim::SingleModeMultiPeak<double>{{peak0}};
    auto measure0 = bosim::operation::HomodyneSingleModeX<double>{0};

    constexpr std::int32_t SampleSize = 100000;

    auto sample0 = std::vector<double>{};
    sample0.reserve(SampleSize);
    for (std::int32_t i = 0; i < SampleSize; ++i) {
        auto state0 = bosim::State<double>{{mode0}};
        state0.SetSeed(static_cast<std::uint64_t>(i));
        sample0.push_back(measure0(state0));
    }
    auto ofs0 = std::ofstream{"HomodyneSingleModeXSinglePeak0.txt"};
    if (!ofs0.bad()) {
        for (const auto val : sample0) { ofs0 << val << "\n"; }
        ofs0.close();
    }

    //                   ⎡ -2 + 0i ⎤      ⎡ 0.25  0.5 ⎤
    // c = 0.5 + 0i, v = ⎢         ⎥, m = ⎢           ⎥
    //                   ⎣  1 + 0i ⎦      ⎣ 0.5    1  ⎦
    const auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays(
        {0.5, 0}, {-2, 0, 1, 0}, {0.25, 0.5, 0.5, 1});
    const auto mode1 = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};

    auto sample1 = std::vector<double>{};
    sample1.reserve(SampleSize);
    for (std::int32_t i = 0; i < SampleSize; ++i) {
        auto state1 = bosim::State<double>{{mode1}};
        state1.SetSeed(static_cast<std::uint64_t>(i));
        sample1.push_back(measure0(state1));
    }
    auto ofs1 = std::ofstream{"HomodyneSingleModeXSinglePeak1.txt"};
    if (!ofs1.bad()) {
        for (const auto val : sample1) { ofs1 << val << "\n"; }
        ofs1.close();
    }

    //                    ⎡ 4 + 0i ⎤      ⎡ 0.25  0.5 ⎤
    // c = 0.25 + 0i, v = ⎢        ⎥, m = ⎢           ⎥
    //                    ⎣ 1 + 0i ⎦      ⎣ 0.5    1  ⎦
    const auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays(
        {0.25, 0}, {4, 0, 1, 0}, {0.25, 0.5, 0.5, 1});
    const auto mode2 = bosim::SingleModeMultiPeak<double>{{peak0, peak1, peak2}};

    auto measure1 = bosim::operation::HomodyneSingleModeX<double>{1};

    auto sample2_0 = std::vector<double>{};
    sample2_0.reserve(SampleSize);
    for (std::int32_t i = 0; i < SampleSize; ++i) {
        auto state2 = bosim::State<double>{{mode1, mode2}};
        state2.SetSeed(static_cast<std::uint64_t>(i));
        sample2_0.push_back(measure0(state2));
    }
    auto ofs2_0 = std::ofstream{"HomodyneSingleModeXSinglePeak2_0.txt"};
    if (!ofs2_0.bad()) {
        for (const auto val : sample2_0) { ofs2_0 << val << "\n"; }
        ofs2_0.close();
    }

    auto sample2_1 = std::vector<double>{};
    sample2_1.reserve(SampleSize);
    for (std::int32_t i = 0; i < SampleSize; ++i) {
        auto state2 = bosim::State<double>{{mode1, mode2}};
        state2.SetSeed(static_cast<std::uint64_t>(i));
        sample2_1.push_back(measure1(state2));
    }
    auto ofs2_1 = std::ofstream{"HomodyneSingleModeXSinglePeak2_1.txt"};
    if (!ofs2_1.bad()) {
        for (const auto val : sample2_1) { ofs2_1 << val << "\n"; }
        ofs2_1.close();
    }

    constexpr std::int32_t CatSampleSize = 2000;

    auto sample_cat = std::vector<double>{};
    sample_cat.reserve(CatSampleSize);
    for (std::int32_t i = 0; i < CatSampleSize; ++i) {
        auto cat_state = bosim::State<double>::GenerateCat(std::complex<double>{2.0, 0}, 0);
        cat_state.SetSeed(static_cast<std::uint64_t>(i));
        sample_cat.push_back(measure0(cat_state));
    }
    auto ofs_cat = std::ofstream{"cat_sample_x.txt"};
    if (!ofs_cat.bad()) {
        for (const auto val : sample_cat) { ofs_cat << val << "\n"; }
        ofs_cat.close();
    }

    auto sample_cat90 = std::vector<double>{};
    sample_cat90.reserve(CatSampleSize);
    for (std::int32_t i = 0; i < CatSampleSize; ++i) {
        auto cat_state90 = bosim::State<double>::GenerateCat(std::complex<double>{0, 2.0}, 0);
        cat_state90.SetSeed(static_cast<std::uint64_t>(i));
        sample_cat90.push_back(measure0(cat_state90));
    }
    auto ofs_cat90 = std::ofstream{"cat_sample_p.txt"};
    if (!ofs_cat90.bad()) {
        for (const auto val : sample_cat90) { ofs_cat90 << val << "\n"; }
        ofs_cat90.close();
    }
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
TEST(Operation, HomodyneSingleMode1ModeSqueezed) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    // Generate a single squeezed state
    auto config = std::map<std::string, nlohmann::json>{};
    constexpr double SqueezingLevel = 10.0;
    constexpr auto Phi = 5 * std::numbers::pi_v<double> / 6;
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

    // Post-selection
    auto state_cp = state;
    config["select"] = 0.3;
    auto measure0 = bosim::operation::HomodyneSingleMode<double>{0, Theta};
    measure0(state_cp, config["select"]);
    const auto json_file_after =
        fmt::format("{}_{}_after.json", test_info->test_suite_name(), test_info->name());
    state_cp.DumpToJson(json_file_after);

    // Random measurement
    constexpr std::size_t NShots = 10000;
    auto measured_vals = std::vector<double>(NShots);
    for (std::size_t i = 0; i < NShots; ++i) {
        auto state_cp = state;
        state_cp.SetSeed(i);
        state_cp.SetShot(i);
        measured_vals[i] = measure0(state_cp);
    }
    config["measured_vals"] = measured_vals;
    AddToJson(json_file_after, config);
    TestInPython("operation", "test_1squeezed_homodyne", json_file_after);
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
TEST(Operation, HomodyneSingleModeX1ModeCat) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    auto config = std::map<std::string, nlohmann::json>{};

    // Generate a single cat state, read it in Python, and compare it with the one defined in
    // Strawberry Fields.
    config["state"]["real"] = 2.0;
    config["state"]["imag"] = 0.0;
    const auto& config_state =
        std::complex<double>{config["state"]["real"], config["state"]["imag"]};
    auto cat_state = bosim::State<double>::GenerateCat(config_state, 0);

    const auto json_file_before =
        fmt::format("{}_{}_before.json", test_info->test_suite_name(), test_info->name());
    cat_state.DumpToJson(json_file_before);
    AddToJson(json_file_before, config);
    TestInPython("state", "test_1cat", json_file_before);

    // Test of a single cat state subject to X homodyne measurement:
    // 1. Post-selection
    // Copy the cat state, perform x-basis homodyne measurement post-selection at
    // a specific value, and compare the output state with the result from Strawberry Fields.
    // 2. Random measurement
    // Repeat copying the cat state and performing x-basis homodyne measurement
    // multiple times, then statistically compare the measured values with the results from
    // Strawberry Fields.

    // Post-selection
    auto state_cp = cat_state;
    config["select"] = 0.5;
    auto measure0 = bosim::operation::HomodyneSingleModeX<double>{0};
    measure0(state_cp, config["select"]);
    const auto json_file_after =
        fmt::format("{}_{}_after.json", test_info->test_suite_name(), test_info->name());
    state_cp.DumpToJson(json_file_after);

    // Random measurement
    constexpr std::size_t NShots = 1000;
    auto measured_vals = std::vector<double>(NShots);
    for (std::size_t i = 0; i < NShots; ++i) {
        auto state_cp = cat_state;
        state_cp.SetSeed(i);
        state_cp.SetShot(i);
        measured_vals[i] = measure0(state_cp);
    }
    config["measured_vals"] = measured_vals;
    AddToJson(json_file_after, config);
    TestInPython("operation", "test_1cat_homodyne", json_file_after);
}

TEST(Operation, HomodyneSingleModeXPostSelection) {
    auto measure0 = bosim::operation::HomodyneSingleModeX<double>{0};
    auto cat_state = bosim::State<double>::GenerateCat(std::complex<double>{0, 2.0}, 0);
    cat_state.SetSeed(static_cast<std::uint64_t>(0));
    EXPECT_DOUBLE_EQ(measure0(cat_state, 1.5), 1.5);
    EXPECT_DOUBLE_EQ(measure0(cat_state, 1.5), 1.5);
    EXPECT_DOUBLE_EQ(measure0(cat_state, 1.8), 1.8);
    EXPECT_DOUBLE_EQ(measure0(cat_state, 1.8), 1.8);
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
    auto select = nlohmann::json{};
    select[MeasuredMode] = 0.3;
    config["select"] = select;
    auto homodyne = bosim::operation::HomodyneSingleModeX<double>{MeasuredMode};

    // Random measurement
    auto random_measurement = [&state, &homodyne, &config](const std::string& json_file) {
        constexpr std::size_t NShots = 1000;
        auto measured_vals = std::vector<double>(NShots);
        for (std::size_t i = 0; i < NShots; ++i) {
            auto state_cp = state;
            state_cp.SetSeed(i);
            state_cp.SetShot(i);
            measured_vals[i] = homodyne(state_cp);
        }
        config["measured_vals"] = measured_vals;

        AddToJson(json_file, config);
        TestInPython("operation", "test_two_cats_bs_homodyne", json_file);
    };

    // Post-selection
    {
        const auto json_file =
            fmt::format("{}_{}_two_cats_bs_homodyne.json", test_suite_name, name);
        auto state_cp = state;
        homodyne(state_cp, config["select"][MeasuredMode]);
        state_cp.DumpToJson(json_file);
        AddWignerToJson<double>(json_file, state_cp, {0, 1});
        random_measurement(json_file);
    }

    // Post-selection in another way
    {
        const auto json_file =
            fmt::format("{}_{}_two_cats_bs_homodyne_ps.json", test_suite_name, name);
        auto state_cp = state;
        auto homodyne_ps = bosim::operation::HomodyneSingleModeX<double>{
            MeasuredMode, config["select"][MeasuredMode]};
        homodyne_ps(state_cp);
        state_cp.DumpToJson(json_file);
        AddWignerToJson<double>(json_file, state_cp, {0, 1});
        random_measurement(json_file);
    }
}

// Test of a state subject to homodyne measurement:
// 1. Post-selection
// Copy the state, perform homodyne measurement post-selection at a specific value, and compare the
// output state with the result from Strawberry Fields.
// 2. Random measurement
// Repeat copying the state and performing homodyne measurement multiple times, then statistically
// compare the measured values with the results from Strawberry Fields.
void HomodyneSingleMode2Mode(std::map<std::string, nlohmann::json>& config,
                             const bosim::State<double>& state, std::string test_suite_name,
                             std::string name) {
    constexpr std::size_t MeasuredMode = 0;
    auto select = nlohmann::json{};
    select[MeasuredMode] = 0.3;
    config["select"] = select;
    config["homodyne_angle_theta"] = std::numbers::pi_v<double> / 6;
    auto homodyne =
        bosim::operation::HomodyneSingleMode<double>{MeasuredMode, config["homodyne_angle_theta"]};

    // Post-selection
    auto state_cp = state;
    homodyne(state_cp, config["select"][MeasuredMode]);
    const auto json_file =
        fmt::format("{}_{}_two_cats_bs_homodyne_theta.json", test_suite_name, name);
    state_cp.DumpToJson(json_file);
    AddWignerToJson<double>(json_file, state_cp, {0, 1});

    // Random measurement
    constexpr std::size_t NShots = 1000;
    auto measured_vals = std::vector<double>(NShots);
    for (std::size_t i = 0; i < NShots; ++i) {
        auto state_cp = state;
        state_cp.SetSeed(i);
        state_cp.SetShot(i);
        measured_vals[i] = homodyne(state_cp);
    }
    config["measured_vals"] = measured_vals;

    AddToJson(json_file, config);
    TestInPython("operation", "test_two_cats_bs_homodyne_theta", json_file);
}

/*
Create JSON data in the following format, read it in Python, and run tests.
{
    "bs": {"theta": float, "phi": float},
    "mode0": int,
    "mode1": int,
    "matrix": [float...],
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
}
*/
TEST(Operation, HomodyneSingleMode2Mode) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    auto config = std::map<std::string, nlohmann::json>{};

    // Beamsplitter test:
    // Create a beam splitter with specific parameters, read it in Python, and compare its operation
    // matrix with the theoretically expected one.
    constexpr auto Pi = std::numbers::pi_v<double>;
    config["bs"]["theta"] = -Pi / 3;
    config["bs"]["phi"] = Pi / 4;
    const double theta = config["bs"]["theta"];
    const double phi = config["bs"]["phi"];
    auto bs = bosim::operation::BeamSplitter<double>{0, 1, std::abs(std::cos(theta)), theta};

    const auto json_file_bs =
        fmt::format("{}_{}_bs.json", test_info->test_suite_name(), test_info->name());
    bs.DumpToJson(json_file_bs);
    AddToJson(json_file_bs, config);
    TestInPython("operation", "test_bs", json_file_bs);

    // Test of two cat states subject to beamsplitter:
    // Generate two cat states, read it in Python, and compare it with the one defined in
    // Strawberry Fields.
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

    const auto json_file_two_cats =
        fmt::format("{}_{}_two_cats.json", test_info->test_suite_name(), test_info->name());
    state.DumpToJson(json_file_two_cats);
    AddToJson(json_file_two_cats, config);
    AddWignerToJson<double>(json_file_two_cats, state, {0, 1});
    TestInPython("state", "test_two_cats", json_file_two_cats);

    // Construct a different type of beam splitter by combining phase rotation operations and a beam
    // splitter, apply it to the two cat states, read the output state in Python, and compare it
    // with the output state obtained by running an equivalent circuit in Strawberry Fields.
    auto rot_front0 = bosim::operation::PhaseRotation<double>{0, phi - Pi};
    auto rot_front1 = bosim::operation::PhaseRotation<double>{1, -Pi / 2};
    auto rot_back0 = bosim::operation::PhaseRotation<double>{0, Pi - theta - phi};
    auto rot_back1 = bosim::operation::PhaseRotation<double>{1, Pi / 2 - theta};
    rot_front0(state);
    rot_front1(state);
    bs(state);
    rot_back0(state);
    rot_back1(state);

    const auto json_file_two_cats_bs =
        fmt::format("{}_{}_two_cats_bs.json", test_info->test_suite_name(), test_info->name());
    state.DumpToJson(json_file_two_cats_bs);
    AddToJson(json_file_two_cats_bs, config);
    AddWignerToJson<double>(json_file_two_cats_bs, state, {0, 1});
    TestInPython("operation", "test_two_cats_bs", json_file_two_cats_bs);

    // Test measurement
    HomodyneSingleModeX2Mode(config, state, test_info->test_suite_name(), test_info->name());
    HomodyneSingleMode2Mode(config, state, test_info->test_suite_name(), test_info->name());
}

TEST(Operation, UpdateHomodyneAngle) {
    auto homodyne = bosim::operation::HomodyneSingleMode<double>{0, 0};
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        homodyne.GetMutTheta() = 0;
        EXPECT_NEAR(homodyne(state), -10, 2);
    }
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        homodyne.GetMutTheta() = std::numbers::pi_v<double> / 2;
        EXPECT_NEAR(homodyne(state), 10, 2);
    }
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        homodyne.GetMutTheta() = -std::numbers::pi_v<double> / 2;
        EXPECT_NEAR(homodyne(state), -10, 2);
    }
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        homodyne.GetMutTheta() = std::numbers::pi_v<double>;
        EXPECT_NEAR(homodyne(state), 10, 2);
    }
}

TEST(Operation, UpdateHomodyneAngleCPUMultiThreadPeakLevel) {
    auto circuit = bosim::Circuit<double>{};

    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, 0);
    const auto settings = bosim::SimulateSettings{1, bosim::Backend::CPUMultiThreadPeakLevel,
                                                  bosim::StateSaveMethod::None};
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::HomodyneSingleMode<double>>) {
                    op_variant->GetMutTheta() = 0;
                } else {
                    FAIL();
                }
            },
            circuit.GetMutOperations().at(0));
        EXPECT_NEAR(bosim::Simulate(settings, circuit, state).result[0].measured_values.at(0), -10,
                    2);
    }
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::HomodyneSingleMode<double>>) {
                    op_variant->GetMutTheta() = std::numbers::pi_v<double> / 2;
                } else {
                    FAIL();
                }
            },
            circuit.GetMutOperations().at(0));
        EXPECT_NEAR(bosim::Simulate(settings, circuit, state).result[0].measured_values.at(0), 10,
                    2);
    }
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::HomodyneSingleMode<double>>) {
                    op_variant->GetMutTheta() = -std::numbers::pi_v<double> / 2;
                } else {
                    FAIL();
                }
            },
            circuit.GetMutOperations().at(0));
        EXPECT_NEAR(bosim::Simulate(settings, circuit, state).result[0].measured_values.at(0), -10,
                    2);
    }
    {
        auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{10, -10});
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::HomodyneSingleMode<double>>) {
                    op_variant->GetMutTheta() = std::numbers::pi_v<double>;
                } else {
                    FAIL();
                }
            },
            circuit.GetMutOperations().at(0));
        EXPECT_NEAR(bosim::Simulate(settings, circuit, state).result[0].measured_values.at(0), 10,
                    2);
    }
}

// Comparison between the result of applying the util::BeamSplitter50 operation to coherent states
// and the ideal state
TEST(Operation, BeamSplitter50) {
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    auto bs = bosim::operation::util::BeamSplitter50<double>{0, 1};
    bs(state);

    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{std::sqrt(2), 0}, std::complex<double>{0, 0},
        std::complex<double>{std::sqrt(2), 0}, std::complex<double>{0, 0};
    const auto expected_covariance =
        (bosim::Hbar<double> / 2) * bosim::MatrixD<double>::Identity(4, 4);

    for (long j = 0; j < 4; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 4; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
}

// Comparison between the result of applying the util::BeamSplitterStd operation to coherent states
// and the ideal state
TEST(Operation, BeamSplitterStd) {
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    constexpr double Theta = std::numbers::pi_v<double> / 6;
    constexpr double Phi = std::numbers::pi_v<double> / 4;
    auto bs = bosim::operation::util::BeamSplitterStd<double>{0, 1, Theta, Phi};
    bs(state);

    const auto c = std::cos(Theta);
    const auto sc = std::sin(Theta) * std::cos(Phi);
    const auto ss = std::sin(Theta) * std::sin(Phi);
    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{c + sc + ss, 0}, std::complex<double>{sc - ss - c, 0},
        std::complex<double>{c + sc - ss, 0}, std::complex<double>{sc + ss - c, 0};
    const auto expected_covariance = (bosim::Hbar<double> / 2) * (c * c + sc * sc + ss * ss) *
                                     bosim::MatrixD<double>::Identity(4, 4);

    for (long j = 0; j < 4; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 4; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
}

// Comparison between the result of applying the ControlledZ operation to coherent states,
// followed by applying ReplaceBySqueezedState to mode 0, and the ideal state
TEST(Operation, ReplaceBySqueezedState) {
    auto mode0 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{1, 1});
    auto mode1 = bosim::SingleModeMultiPeak<double>::GenerateCoherent(std::complex<double>{-1, -1});
    auto state = bosim::State<double>{{mode0, mode1}};
    constexpr double G = 2;
    constexpr double SqueezingLevel = 15;
    auto controlled_z = bosim::operation::ControlledZ<double>{0, 1, G};
    controlled_z(state);
    auto op = bosim::operation::util::ReplaceBySqueezedState<double>{0, SqueezingLevel};
#ifndef NDEBUG
    EXPECT_THROW(op(state), std::runtime_error);
#else
    // Zero out the off-diagonal elements
    state.GetMutCov(0)(0, 1) = 0;
    state.GetMutCov(0)(0, 3) = 0;
    state.GetMutCov(0)(1, 0) = 0;
    state.GetMutCov(0)(1, 2) = 0;
    state.GetMutCov(0)(2, 1) = 0;
    state.GetMutCov(0)(2, 3) = 0;
    state.GetMutCov(0)(3, 0) = 0;
    state.GetMutCov(0)(3, 2) = 0;
    EXPECT_NO_THROW(op(state));

    const auto squeezing_level_no_unit = std::pow(10.0, SqueezingLevel * 0.1);
    auto expected_mean = bosim::VectorC<double>{4};
    expected_mean << std::complex<double>{0, 0}, std::complex<double>{-1, 0},
        std::complex<double>{0, 0}, std::complex<double>{G - 1, 0};
    auto expected_covariance = bosim::MatrixD<double>{4, 4};
    expected_covariance << 1 / squeezing_level_no_unit, 0, 0, 0, 0, 1, 0, 0, 0, 0,
        squeezing_level_no_unit, 0, 0, 0, 0, 1 + G * G;
    expected_covariance *= bosim::Hbar<double> / 2;

    for (long j = 0; j < 4; ++j) {
        EXPECT_NEAR(state.GetMean(0)(j).real(), expected_mean(j).real(), 1e-9);
        EXPECT_NEAR(state.GetMean(0)(j).imag(), expected_mean(j).imag(), 1e-9);
        for (long k = 0; k < 4; ++k) {
            EXPECT_NEAR(state.GetCov(0)(j, k), expected_covariance(j, k), 1e-9);
        }
    }
#endif
}

TEST(Operation, PythonScript) { TestInPython("operation", "test_python", "3", "6"); }
