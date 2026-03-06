#pragma once

#include <Eigen/Dense>

#include <cmath>
#include <complex>
#include <concepts>

#include "bosim/base/float.h"
#include "bosim/exception.h"

namespace bosim {
template <std::floating_point Float>
using Vector2D = Eigen::Vector2<Float>;

template <std::floating_point Float>
using Vector2C = Eigen::Vector2<std::complex<Float>>;

template <std::floating_point Float>
using Matrix2D = Eigen::Matrix2<Float>;

template <std::floating_point Float>
using VectorC = Eigen::VectorX<std::complex<Float>>;

template <std::floating_point Float>
using MatrixD = Eigen::MatrixX<Float>;

template <typename T>
concept EigenMatrix = requires(T m) {
    typename T::Scalar;
    { T::RowsAtCompileTime } -> std::convertible_to<int>;
    { T::ColsAtCompileTime } -> std::convertible_to<int>;
    { m.rows() } -> std::convertible_to<int>;
    { m.cols() } -> std::convertible_to<int>;
};

template <EigenMatrix Matrix>
inline bool IsSquare(const Matrix& m) {
    return m.rows() == m.cols();
}
template <EigenMatrix Matrix>
inline bool IsSymmetric(const Matrix& m) {
    using Float = typename Matrix::Scalar;
    if (!IsSquare(m)) return false;
    return (m - m.transpose()).array().abs().maxCoeff() < Eps<Float>;
}

template <EigenMatrix Matrix>
inline bool IsPositiveSemidefinite(const Matrix& m) {
    using Float = typename Matrix::Scalar;
    if (!IsSymmetric(m)) return false;

    auto solver{Eigen::SelfAdjointEigenSolver<Matrix>(m)};
    if (solver.info() != Eigen::Success) {
        throw SimulationError(
            error::InvalidState,
            "Failed to compute eigenvalues when checking if the matrix is positive semidefinite");
    }

    auto eigenvalues{solver.eigenvalues()};
    return (eigenvalues.array() >= -Eps<Float>).all();
}

template <std::floating_point Float>
std::complex<Float> General1DGaussianPdf(const Float x, const std::complex<Float> mu,
                                         const Float sigma) {
    const std::complex<Float> exponent =
        -Float{0.5} * std::pow((std::complex<Float>{x, 0} - mu) / sigma, Float{2});
    return (1 / (sigma * std::sqrt(2 * std::numbers::pi_v<Float>))) * std::exp(exponent);
}

template <std::floating_point Float>
class General2DGaussianPdf {
public:
    General2DGaussianPdf(const Vector2C<Float>& mu, const Matrix2D<Float>& cov)
        : mu_(mu), cov_(cov) {}

    std::complex<Float> operator()(const Float x, const Float p) const {
        const auto xi = Vector2C<Float>{std::complex<Float>{x, 0}, std::complex<Float>{p, 0}};
        const std::complex<Float> exponent =
            -0.5 * (xi - mu_).transpose() * cov_.inverse() * (xi - mu_);
        return std::exp(exponent) / std::sqrt((2 * std::numbers::pi_v<Float> * cov_).determinant());
    }

private:
    Vector2C<Float> mu_;
    Matrix2D<Float> cov_;
};

/**
 * @brief Checks whether two floating-point values are equivalent modulo \f$ \pi \f$.
 *
 * @tparam Float
 * @param val1 The first value to compare.
 * @param val2 The second value to compare.
 * @param eps The tolerance for equivalence. Defaults to \f$ 10^{-6} \f$.
 * @return true if the values are equivalent modulo \f$ \pi \f$, false otherwise.
 */
template <std::floating_point Float>
inline bool IsEquivModPi(const Float val1, const Float val2,
                         const Float eps = static_cast<Float>(1e-6)) {
    const Float quotient = (val1 - val2) / std::numbers::pi_v<Float>;
    const Float rounded = std::round(quotient);
    return std::abs(quotient - rounded) < eps;
}

}  // namespace bosim
