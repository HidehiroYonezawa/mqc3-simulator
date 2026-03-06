#include "bosim/base/math.h"

#include <gtest/gtest.h>

TEST(Math, Vector2C) {
    const auto u =
        bosim::Vector2C<double>{std::complex<double>{1.0, 0}, std::complex<double>{1.0, 0}};
    EXPECT_DOUBLE_EQ(1.0, u[0].real());
    EXPECT_DOUBLE_EQ(0.0, u[0].imag());
    EXPECT_DOUBLE_EQ(1.0, u[1].real());
    EXPECT_DOUBLE_EQ(0.0, u[1].imag());

    const auto w = bosim::Vector2C<double>{std::sqrt(2.0) * std::complex<double>{1.0, 0},
                                           std::sqrt(2.0) * std::complex<double>{1.0, 0}};
    EXPECT_DOUBLE_EQ(std::sqrt(2.0), w[0].real());
    EXPECT_DOUBLE_EQ(0.0, w[0].imag());
    EXPECT_DOUBLE_EQ(std::sqrt(2.0), w[1].real());
    EXPECT_DOUBLE_EQ(0.0, w[1].imag());

    // FIXME: Next tests will fail if build with g++.
    // const auto v = std::sqrt(2.0) * bosim::Vector2C<double>{std::complex<double>{1.0, 0},
    //                                                         std::complex<double>{1.0, 0}};
    // EXPECT_DOUBLE_EQ(std::sqrt(2.0), v[0].real());
    // EXPECT_DOUBLE_EQ(0.0, v[0].imag());
    // EXPECT_DOUBLE_EQ(std::sqrt(2.0), v[1].real());
    // EXPECT_DOUBLE_EQ(0.0, v[1].imag());
}
TEST(Math, PositiveSemidefiniteMatrixCheck) {
    auto m_2d = bosim::Matrix2D<double>{};
    m_2d << 1, 2, 2, 4;
    EXPECT_TRUE(bosim::IsSquare<bosim::Matrix2D<double>>(m_2d));
    EXPECT_TRUE(bosim::IsSymmetric<bosim::Matrix2D<double>>(m_2d));
    EXPECT_TRUE(bosim::IsPositiveSemidefinite<bosim::Matrix2D<double>>(m_2d));

    auto m_3d = bosim::MatrixD<double>{3, 3};
    m_3d << 1, 1, 0, 1, 2, 1, 0, 1, 1;
    EXPECT_TRUE(bosim::IsSquare<bosim::MatrixD<double>>(m_3d));
    EXPECT_TRUE(bosim::IsSymmetric<bosim::MatrixD<double>>(m_3d));
    EXPECT_TRUE(bosim::IsPositiveSemidefinite<bosim::MatrixD<double>>(m_3d));
}

TEST(Math, SymmetricMatrixCheck) {
    auto m_2d = bosim::Matrix2D<double>{};
    m_2d << 1, 2, 2, -1;
    EXPECT_TRUE(bosim::IsSquare<bosim::Matrix2D<double>>(m_2d));
    EXPECT_TRUE(bosim::IsSymmetric<bosim::Matrix2D<double>>(m_2d));
    EXPECT_FALSE(bosim::IsPositiveSemidefinite<bosim::Matrix2D<double>>(m_2d));

    auto m_3d = bosim::MatrixD<double>{3, 3};
    m_3d << 1, 2, 0, 2, -3, 1, 0, 1, 1;
    EXPECT_TRUE(bosim::IsSquare<bosim::MatrixD<double>>(m_3d));
    EXPECT_TRUE(bosim::IsSymmetric<bosim::MatrixD<double>>(m_3d));
    EXPECT_FALSE(bosim::IsPositiveSemidefinite<bosim::MatrixD<double>>(m_3d));
}

TEST(Math, SquareMatrixCheck) {
    auto m_2d = bosim::Matrix2D<double>{};
    m_2d << 1, 2, 3, 4;
    EXPECT_TRUE(bosim::IsSquare<bosim::Matrix2D<double>>(m_2d));
    EXPECT_FALSE(bosim::IsSymmetric<bosim::Matrix2D<double>>(m_2d));
    EXPECT_FALSE(bosim::IsPositiveSemidefinite<bosim::Matrix2D<double>>(m_2d));

    auto m_3d = bosim::MatrixD<double>{3, 3};
    m_3d << 1, 2, 3, 4, 5, 6, 7, 8, 9;
    EXPECT_TRUE(bosim::IsSquare<bosim::MatrixD<double>>(m_3d));
    EXPECT_FALSE(bosim::IsSymmetric<bosim::MatrixD<double>>(m_3d));
    EXPECT_FALSE(bosim::IsPositiveSemidefinite<bosim::MatrixD<double>>(m_3d));
}

TEST(Math, GeneralMatrixCheck) {
    auto m = bosim::MatrixD<double>{3, 2};
    m << 1, 2, 3, 4, 5, 6;
    EXPECT_FALSE(bosim::IsSquare<bosim::MatrixD<double>>(m));
    EXPECT_FALSE(bosim::IsSymmetric<bosim::MatrixD<double>>(m));
    EXPECT_FALSE(bosim::IsPositiveSemidefinite<bosim::MatrixD<double>>(m));
}
