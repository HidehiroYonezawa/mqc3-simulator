#include "bosim/state.h"

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <unordered_map>
#include <utility>

#include "bosim/base/math.h"

#include "utility.h"

TEST(State, SingleModeSinglePeak) {
    //                 ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto c = std::complex<double>{1, 2};
    auto v = bosim::Vector2C<double>{std::complex<double>{1, 0}, std::complex<double>{2, 0}};
    auto m = bosim::Matrix2D<double>{};
    m << 1, 2, 2, 4;
    auto single_mode_single_peak = bosim::SingleModeSinglePeak<double>{c, v, m};
    EXPECT_EQ(bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                          {1, 2, 2, 4}),
              single_mode_single_peak);
    EXPECT_EQ(single_mode_single_peak.coeff.real(), 1);
    EXPECT_EQ(single_mode_single_peak.coeff.imag(), 2);
    EXPECT_EQ(single_mode_single_peak.mean(0).real(), 1);
    EXPECT_EQ(single_mode_single_peak.mean(0).imag(), 0);
    EXPECT_EQ(single_mode_single_peak.mean(1).real(), 2);
    EXPECT_EQ(single_mode_single_peak.mean(1).imag(), 0);
    EXPECT_EQ(single_mode_single_peak.cov(0, 0), 1);
    EXPECT_EQ(single_mode_single_peak.cov(0, 1), 2);
    EXPECT_EQ(single_mode_single_peak.cov(1, 0), 2);
    EXPECT_EQ(single_mode_single_peak.cov(1, 1), 4);

    //                   ⎡ 1 + 0i ⎤       ⎡ 1  0 ⎤
    // c -> 2 + 3i, v -> ⎢        ⎥, m -> ⎢      ⎥
    //                   ⎣ 0 + 1i ⎦       ⎣ 0  4 ⎦
    c = std::complex<double>{2, 3};
    v(1) = std::complex<double>{0, 1};
    m(0, 1) = 0;
    m(1, 0) = 0;
    single_mode_single_peak.coeff = c;
    single_mode_single_peak.mean = v;
    single_mode_single_peak.cov = m;
    EXPECT_EQ(bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                          {1, 0, 0, 4}),
              single_mode_single_peak);
    EXPECT_EQ(single_mode_single_peak.coeff.real(), 2);
    EXPECT_EQ(single_mode_single_peak.coeff.imag(), 3);
    EXPECT_EQ(single_mode_single_peak.mean(0).real(), 1);
    EXPECT_EQ(single_mode_single_peak.mean(0).imag(), 0);
    EXPECT_EQ(single_mode_single_peak.mean(1).real(), 0);
    EXPECT_EQ(single_mode_single_peak.mean(1).imag(), 1);
    EXPECT_EQ(single_mode_single_peak.cov(0, 0), 1);
    EXPECT_EQ(single_mode_single_peak.cov(0, 1), 0);
    EXPECT_EQ(single_mode_single_peak.cov(1, 0), 0);
    EXPECT_EQ(single_mode_single_peak.cov(1, 1), 4);

    //      ⎡ 1  0 ⎤
    // m -> ⎢      ⎥
    //      ⎣ 2  4 ⎦
    m(1, 0) = 2;
    EXPECT_THROW(bosim::SingleModeSinglePeak<double>(c, v, m), std::invalid_argument);
    EXPECT_THROW(bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 2, 4}),
                 std::invalid_argument);
}

TEST(State, SingleModeMultiPeak) {
    //                  ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v -> ⎢        ⎥, m = ⎢      ⎥
    //                  ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});

    //                 ⎡ 1 + 0i ⎤      ⎡ 1  0 ⎤
    // c = 2 + 3i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  4 ⎦
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});

    auto single_mode_multi_peak = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};
}

TEST(State, MultiModeSinglePeakPlainConstructor) {
    //                 ⎡ 1 + 2i ⎤      ⎡  1  -1   2   0 ⎤
    //                 ⎢        ⎥,     ⎢                ⎥
    //                 ⎢ 3 + 4i ⎥,     ⎢ -1   4  -1   1 ⎥
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢                ⎥
    //                 ⎢ 5 + 6i ⎥,     ⎢  2  -1   6  -2 ⎥
    //                 ⎢        ⎥,     ⎢                ⎥
    //                 ⎣ 7 + 8i ⎦      ⎣  0   1  -2   4 ⎦
    auto c = std::complex<double>{1, 2};
    auto v = bosim::VectorC<double>{4};
    auto m = bosim::MatrixD<double>{4, 4};
    for (int i = 0; i < 4; ++i) {
        v(i) = std::complex<double>{static_cast<double>(2 * i + 1), static_cast<double>(2 * i + 2)};
    }
    m << 1, -1, 2, 0, -1, 4, -1, 1, 2, -1, 6, -2, 0, 1, -2, 4;
    auto multi_mode_single_peak = bosim::MultiModeSinglePeak<double>{c, v, m};
    EXPECT_EQ(multi_mode_single_peak.NumModes(), 2);
    EXPECT_EQ(multi_mode_single_peak.GetCoeff().real(), 1);
    EXPECT_EQ(multi_mode_single_peak.GetCoeff().imag(), 2);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(0).real(), 1);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(0).imag(), 2);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(2).real(), 5);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(2).imag(), 6);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(0, 1), -1);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(2, 0), 2);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(2, 2), 6);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(3, 3), 4);

    //                   ⎡ 1 + 2i ⎤       ⎡  1  -1   2   0 ⎤
    //                   ⎢        ⎥,      ⎢                ⎥
    //                   ⎢ 3 + 4i ⎥,      ⎢ -1   4  -1   1 ⎥
    // c -> 3 + 4i, v -> ⎢        ⎥, m -> ⎢                ⎥
    //                   ⎢ 8 + 7i ⎥,      ⎢  2  -1   6  -2 ⎥
    //                   ⎢        ⎥,      ⎢                ⎥
    //                   ⎣ 7 + 8i ⎦       ⎣  0   1  -2   5 ⎦
    c = std::complex<double>{3, 4};
    v(2) = std::complex<double>{8, 7};
    m(3, 3) = 5;
    multi_mode_single_peak.SetCoeff(c);
    multi_mode_single_peak.SetMean(v);
    multi_mode_single_peak.SetCov(m);
    EXPECT_EQ(multi_mode_single_peak.NumModes(), 2);
    EXPECT_EQ(multi_mode_single_peak.GetCoeff().real(), 3);
    EXPECT_EQ(multi_mode_single_peak.GetCoeff().imag(), 4);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(0).real(), 1);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(0).imag(), 2);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(2).real(), 8);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(2).imag(), 7);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(0, 1), -1);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(2, 0), 2);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(2, 2), 6);
    EXPECT_EQ(multi_mode_single_peak.GetCov()(3, 3), 5);

    EXPECT_EQ(multi_mode_single_peak.GetMeanX(0), std::complex<double>(1, 2));
    EXPECT_EQ(multi_mode_single_peak.GetMeanX(1), std::complex<double>(3, 4));
    EXPECT_EQ(multi_mode_single_peak.GetMeanP(0), std::complex<double>(8, 7));
    EXPECT_EQ(multi_mode_single_peak.GetMeanP(1), std::complex<double>(7, 8));
    EXPECT_EQ(multi_mode_single_peak.GetCovXX(0, 0), 1);
    EXPECT_EQ(multi_mode_single_peak.GetCovXP(0, 0), 2);
    EXPECT_EQ(multi_mode_single_peak.GetCovPX(0, 0), 2);
    EXPECT_EQ(multi_mode_single_peak.GetCovPP(0, 0), 6);
    EXPECT_EQ(multi_mode_single_peak.GetCovXX(0, 1), -1);
    EXPECT_EQ(multi_mode_single_peak.GetCovXP(0, 1), 0);
    EXPECT_EQ(multi_mode_single_peak.GetCovPX(0, 1), -1);
    EXPECT_EQ(multi_mode_single_peak.GetCovPP(0, 1), -2);
    EXPECT_EQ(multi_mode_single_peak.GetCovXX(1, 0), -1);
    EXPECT_EQ(multi_mode_single_peak.GetCovXP(1, 0), -1);
    EXPECT_EQ(multi_mode_single_peak.GetCovPX(1, 0), 0);
    EXPECT_EQ(multi_mode_single_peak.GetCovPP(1, 0), -2);
    EXPECT_EQ(multi_mode_single_peak.GetCovXX(1, 1), 4);
    EXPECT_EQ(multi_mode_single_peak.GetCovXP(1, 1), 1);
    EXPECT_EQ(multi_mode_single_peak.GetCovPX(1, 1), 1);
    EXPECT_EQ(multi_mode_single_peak.GetCovPP(1, 1), 5);

    // Exception
#ifndef NDEBUG
    EXPECT_THROW(multi_mode_single_peak.GetMeanX(2), std::out_of_range);
    EXPECT_THROW(multi_mode_single_peak.GetMeanP(3), std::out_of_range);
    EXPECT_THROW(multi_mode_single_peak.GetCovXX(2, 2), std::out_of_range);
    EXPECT_THROW(multi_mode_single_peak.GetCovXP(1, 2), std::out_of_range);
    EXPECT_THROW(multi_mode_single_peak.GetCovPX(0, 3), std::out_of_range);
    EXPECT_THROW(multi_mode_single_peak.GetCovPP(2, 0), std::out_of_range);
#endif

    //                 ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto peak_1mode = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays(
        {1, 2}, {1, 0, 2, 0}, {1, 2, 2, 4});
    EXPECT_THROW(multi_mode_single_peak.SetMean(peak_1mode.mean), std::invalid_argument);
    EXPECT_THROW(multi_mode_single_peak.SetCov(peak_1mode.cov), std::invalid_argument);
}

struct PairHash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& pair) const {
        std::size_t hash1 = std::hash<T1>{}(pair.first);
        std::size_t hash2 = std::hash<T2>{}(pair.second);
        return hash1 ^ (hash2 << 1);
    }
};

TEST(State, MultiModeSinglePeakFromSinglePeakModes) {
    //                 ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});

    //                 ⎡ 1 + 0i ⎤      ⎡ 1  0 ⎤
    // c = 2 + 3i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  4 ⎦
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});

    //                 ⎡ 0 + 3i ⎤      ⎡ 1  0 ⎤
    // c = 4 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  0 ⎦
    auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({4, 2}, {0, 3, 0, 1},
                                                                             {1, 0, 0, 0});

    //                    ⎡ 1 + 0i ⎤      ⎡ 1  0  0  2  0  0 ⎤
    //                    ⎢        ⎥      ⎢                  ⎥
    //                    ⎢ 1 + 0i ⎥      ⎢ 0  1  0  0  0  0 ⎥
    //                    ⎢        ⎥      ⎢                  ⎥
    //                    ⎢ 0 + 3i ⎥      ⎢ 0  0  1  0  0  0 ⎥
    // c = -30 + 20i, v = ⎢        ⎥, m = ⎢                  ⎥
    //                    ⎢ 2 + 0i ⎥      ⎢ 2  0  0  4  0  0 ⎥
    //                    ⎢        ⎥      ⎢                  ⎥
    //                    ⎢ 0 + 1i ⎥      ⎢ 0  0  0  0  4  0 ⎥
    //                    ⎢        ⎥      ⎢                  ⎥
    //                    ⎣ 0 + 1i ⎦      ⎣ 0  0  0  0  0  0 ⎦
    auto multi_mode_single_peak = bosim::MultiModeSinglePeak<double>{{peak0, peak1, peak2}};
    EXPECT_EQ(multi_mode_single_peak.NumModes(), 3);
    EXPECT_DOUBLE_EQ(multi_mode_single_peak.GetCoeff().real(), -30);
    EXPECT_DOUBLE_EQ(multi_mode_single_peak.GetCoeff().imag(), 20);
    EXPECT_EQ(multi_mode_single_peak.GetMean()(0), std::complex<double>(1, 0));
    EXPECT_EQ(multi_mode_single_peak.GetMean()(1), std::complex<double>(1, 0));
    EXPECT_EQ(multi_mode_single_peak.GetMean()(2), std::complex<double>(0, 3));
    EXPECT_EQ(multi_mode_single_peak.GetMean()(3), std::complex<double>(2, 0));
    EXPECT_EQ(multi_mode_single_peak.GetMean()(4), std::complex<double>(0, 1));
    EXPECT_EQ(multi_mode_single_peak.GetMean()(5), std::complex<double>(0, 1));
    auto coordinates = std::unordered_map<std::pair<long, long>, double, PairHash>{};
    coordinates[{0, 0}] = 1;
    coordinates[{0, 3}] = 2;
    coordinates[{1, 1}] = 1;
    coordinates[{2, 2}] = 1;
    coordinates[{3, 0}] = 2;
    coordinates[{3, 3}] = 4;
    coordinates[{4, 4}] = 4;
    for (long i = 0; i < 6; ++i) {
        for (long j = 0; j < 6; ++j) {
            if (auto it = coordinates.find({i, j}); it != coordinates.end()) {
                EXPECT_EQ(multi_mode_single_peak.GetCov()(i, j), it->second);
            } else {
                EXPECT_EQ(multi_mode_single_peak.GetCov()(i, j), 0);
            }
        }
    }
}

TEST(State, State) {
    //                 ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});

    //                 ⎡ 1 + 0i ⎤      ⎡ 1  0 ⎤
    // c = 2 + 3i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  4 ⎦
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});

    //                 ⎡ 0 + 3i ⎤      ⎡ 1  0 ⎤
    // c = 4 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  0 ⎦
    auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({4, 2}, {0, 3, 0, 1},
                                                                             {1, 0, 0, 0});

    auto mode0 = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};
    auto mode1 = bosim::SingleModeMultiPeak<double>{{peak0, peak1, peak2}};
    auto mode2 = bosim::SingleModeMultiPeak<double>{{peak1, peak2}};
    auto state = bosim::State<double>{{mode0, mode1, mode2}};
    const std::complex<double> c0 = peak0.coeff;
    const std::complex<double> c1 = peak1.coeff;
    const std::complex<double> c2 = peak2.coeff;
    EXPECT_EQ(state.NumModes(), 3);
    EXPECT_DOUBLE_EQ(state.GetPeaks()[2].GetCoeff().real(), (c0 * c1 * c1).real());
    EXPECT_DOUBLE_EQ(state.GetPeaks()[2].GetCoeff().imag(), (c0 * c1 * c1).imag());
    EXPECT_DOUBLE_EQ(state.GetPeaks()[9].GetCoeff().real(), (c1 * c1 * c2).real());
    EXPECT_DOUBLE_EQ(state.GetPeaks()[9].GetCoeff().imag(), (c1 * c1 * c2).imag());
    EXPECT_EQ(state.NumPeaks(), 12);

    const double sum = abs(c0 * c0 * c1) + abs(c0 * c0 * c2) + abs(c0 * c1 * c1) +
                       abs(c0 * c1 * c2) + abs(c0 * c2 * c1) + abs(c0 * c2 * c2) +
                       abs(c1 * c0 * c1) + abs(c1 * c0 * c2) + abs(c1 * c1 * c1) +
                       abs(c1 * c1 * c2) + abs(c1 * c2 * c1) + abs(c1 * c2 * c2);
    EXPECT_DOUBLE_EQ(state.GetExtent(), sum);
}

TEST(State, Copy) {
    //                 ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});

    //                 ⎡ 1 + 0i ⎤      ⎡ 1  0 ⎤
    // c = 2 + 3i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  4 ⎦
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});

    //                 ⎡ 0 + 3i ⎤      ⎡ 1  0 ⎤
    // c = 4 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  0 ⎦
    auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({4, 2}, {0, 3, 0, 1},
                                                                             {1, 0, 0, 0});

    auto mode0 = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};
    auto mode1 = bosim::SingleModeMultiPeak<double>{{peak0, peak1, peak2}};
    auto mode2 = bosim::SingleModeMultiPeak<double>{{peak1, peak2}};
    auto state = bosim::State<double>{{mode0, mode1, mode2}};

    const auto copied_state = state;  // NOLINT(performance-unnecessary-copy-initialization)
    EXPECT_TRUE(CompareState(state, copied_state));
    EXPECT_EQ(state.GetEngine(), copied_state.GetEngine());
    EXPECT_EQ(state.GetShot(), copied_state.GetShot());
}

TEST(State, Move) {
    //                 ⎡ 1 + 0i ⎤      ⎡ 1  2 ⎤
    // c = 1 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 2 + 0i ⎦      ⎣ 2  4 ⎦
    auto peak0 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({1, 2}, {1, 0, 2, 0},
                                                                             {1, 2, 2, 4});

    //                 ⎡ 1 + 0i ⎤      ⎡ 1  0 ⎤
    // c = 2 + 3i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  4 ⎦
    auto peak1 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({2, 3}, {1, 0, 0, 1},
                                                                             {1, 0, 0, 4});

    //                 ⎡ 0 + 3i ⎤      ⎡ 1  0 ⎤
    // c = 4 + 2i, v = ⎢        ⎥, m = ⎢      ⎥
    //                 ⎣ 0 + 1i ⎦      ⎣ 0  0 ⎦
    auto peak2 = bosim::SingleModeSinglePeak<double>::GenerateFromRealArrays({4, 2}, {0, 3, 0, 1},
                                                                             {1, 0, 0, 0});

    auto mode0 = bosim::SingleModeMultiPeak<double>{{peak0, peak1}};
    auto mode1 = bosim::SingleModeMultiPeak<double>{{peak0, peak1, peak2}};
    auto mode2 = bosim::SingleModeMultiPeak<double>{{peak1, peak2}};
    auto state = bosim::State<double>{{mode0, mode1, mode2}};

    const auto copied_state = state;
    const auto moved_state = std::move(state);
    EXPECT_TRUE(CompareState(copied_state, moved_state));
    EXPECT_EQ(copied_state.GetEngine(), moved_state.GetEngine());
    EXPECT_EQ(copied_state.GetShot(), moved_state.GetShot());
}

TEST(State, SqueezedState) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    auto config = std::map<std::string, nlohmann::json>{};
    config["state"]["squeezing_level"] = 5.0;
    config["state"]["phi"] = 2 * std::numbers::pi_v<double> / 3;
    config["state"]["displacement"]["real"] = 0.0;
    config["state"]["displacement"]["imag"] = 0.0;

    auto sq_state = bosim::State<double>::GenerateSqueezed(
        config["state"]["squeezing_level"], config["state"]["phi"],
        std::complex<double>{config["state"]["displacement"]["real"],
                             config["state"]["displacement"]["imag"]});

    const auto json_file =
        fmt::format("{}_{}.json", test_info->test_suite_name(), test_info->name());
    sq_state.DumpToJson(json_file);
    AddToJson(json_file, config);
    AddWignerToJson<double>(json_file, sq_state, {0}, -5, 5);
    TestInPython("state", "test_squeezed", json_file);
}

TEST(State, VacuumVsSqueezed) {
    const double s = 5.0;
    auto vacuum = bosim::State<double>::GenerateVacuum();
    auto sq_state = bosim::State<double>::GenerateSqueezed(s, 0, std::complex<double>{0, 0});
    EXPECT_DOUBLE_EQ(vacuum.GetCovPP(0, 0, 0) * std::pow(10.0, s / 10), sq_state.GetCovPP(0, 0, 0));
}

TEST(State, CatState) {
    const ::testing::TestInfo* test_info = ::testing::UnitTest::GetInstance()->current_test_info();

    auto config = std::map<std::string, nlohmann::json>{};
    config["state"]["real"] = 4.0 / std::sqrt(2 * bosim::Hbar<double>);
    config["state"]["imag"] = 0.0;
    config["state"]["parity"] = 1;
    const auto& config_state =
        std::complex<double>{config["state"]["real"], config["state"]["imag"]};

    auto cat_state = bosim::State<double>::GenerateCat(config_state, config["state"]["parity"]);

    const auto json_file =
        fmt::format("{}_{}.json", test_info->test_suite_name(), test_info->name());
    cat_state.DumpToJson(json_file);
    AddToJson(json_file, config);
    TestInPython("state", "test_1cat", json_file);
}
