#pragma once

#include <Eigen/Dense>
#include <nlohmann/json.hpp>

#include <cmath>
#include <complex>
#include <concepts>
#include <cstddef>
#include <filesystem>  // NOLINT
#include <fstream>
#include <numeric>
#include <random>
#include <ranges>
#include <thread>
#include <vector>

#include "bosim/base/constant.h"
#include "bosim/base/math.h"
#include "bosim/base/timer.h"

namespace bosim {
/**
 * @brief Gaussian parameters for a single peak in a single mode.
 *
 * @tparam Float
 */
template <std::floating_point Float>
struct SingleModeSinglePeak {
    explicit SingleModeSinglePeak<Float>(const std::complex<Float>& coeff,
                                         const Vector2C<Float>& mean, const Matrix2D<Float>& cov)
        : coeff(coeff), mean(mean), cov(cov) {
        if (mean.size() != 2) {
            throw std::invalid_argument("Invalid mean: expected size 2 vector");
        }
        if (cov.rows() != 2 || cov.cols() != 2) {
            throw std::invalid_argument("Invalid covariance matrix: expected 2x2 matrix");
        }
        if (!IsPositiveSemidefinite(cov)) {
            throw std::invalid_argument(
                "Invalid covariance matrix: it must be symmetric and positive semidefinite");
        }
    }

    std::complex<Float> coeff;
    Vector2C<Float> mean;
    Matrix2D<Float> cov;

    auto operator<=>(const SingleModeSinglePeak&) const = default;

    /**
     * @brief Construct an object from arrays {Re(c), Im(c)}, {Re(mx), Im(mx), Re(mp), Im(mp)}, and
     * {Cxx, Cxp, Cpx, Cpp}, where c, m, and C are coefficient, mean, covariance matrix,
     * respectively.
     *
     * @param coeff Coefficient.
     * @param mean Mean vector.
     * @param cov Covariance matrix.
     * @return SingleModeSinglePeak<Float>
     */
    static SingleModeSinglePeak<Float> GenerateFromRealArrays(const std::array<Float, 2>& coeff,
                                                              const std::array<Float, 4>& mean,
                                                              const std::array<Float, 4>& cov) {
        const auto c = std::complex<Float>{coeff[0], coeff[1]};
        const auto v = Vector2C<Float>{std::complex<Float>{mean[0], mean[1]},
                                       std::complex<Float>{mean[2], mean[3]}};
        auto m = Matrix2D<Float>();
        m << cov[0], cov[1], cov[2], cov[3];
        return SingleModeSinglePeak<Float>{c, v, m};
    }
};

template <std::floating_point Float>
Matrix2D<Float> GenerateSqueezedCov(const Float squeezing_level, const Float phi) {
    const auto squeezing_level_no_unit = static_cast<Float>(std::pow(10.0, squeezing_level * 0.1));
    auto squeezing = Matrix2D<Float>{};
    squeezing << 1 / squeezing_level_no_unit, 0, 0, squeezing_level_no_unit;
    const auto initial = (Hbar<Float> / 2) * squeezing;
    auto rotation = Matrix2D<Float>{};
    const auto c = std::cos(phi);
    const auto s = std::sin(phi);
    rotation << c, -s, s, c;
    return rotation * initial * rotation.transpose();
}

/**
 * @brief Gaussian parameters for multiple peaks in a single mode.
 *
 * @tparam Float
 */
template <std::floating_point Float>
struct SingleModeMultiPeak {
    std::vector<SingleModeSinglePeak<Float>> peaks;

    /**
     * @brief Generate squeezed state.
     *
     * Wigner function is represented by mean
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} = \boldsymbol{d} \f]
     * and covariance matrix
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp} \\ \sigma_{xp} & \sigma_p^2
     * \end{pmatrix} = RS \begin{pmatrix} \hbar/2 & \\ & \hbar/2 \end{pmatrix} S^\text{T}R^\text{T},
     * \f]
     * where
     * \f[
     * S = \begin{pmatrix} \frac{1}{\sqrt{s\text{ dB}}} & \\ & \sqrt{s\text{ dB}} \end{pmatrix}
     * = \begin{pmatrix} 10^{-\frac{s}{20}} & \\ & 10^{\frac{s}{20}} \end{pmatrix},
     * \f]
     * \f[
     * R = \begin{pmatrix} \cos \phi & -\sin \phi \\ \sin \phi & \cos \phi \end{pmatrix}.
     * \f]
     *
     * @param squeezing_level Squeezing level in dB \f$ s \f$.
     * @param phi Squeezing angle \f$ \phi \f$.
     * @param displacement Displacement \f$ \boldsymbol{d} \f$.
     * @return SingleModeMultiPeak<Float>
     */
    static SingleModeMultiPeak<Float> GenerateSqueezed(
        const Float squeezing_level, const Float phi = 0.0,
        const std::complex<Float>& displacement = std::complex<Float>{0, 0}) {
        const auto mu = Vector2C<Float>{std::complex<Float>{displacement.real(), 0},
                                        std::complex<Float>{displacement.imag(), 0}};
        auto peak = SingleModeSinglePeak<Float>{1, mu, GenerateSqueezedCov(squeezing_level, phi)};
        return SingleModeMultiPeak{{peak}};
    }

    /**
     * @brief Generate coherent state.
     *
     * Wigner function is represented by mean
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} = \boldsymbol{d} \f]
     * and covariance matrix
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp} \\ \sigma_{xp} & \sigma_p^2
     * \end{pmatrix} = \begin{pmatrix} \hbar/2 & \\ & \hbar/2 \end{pmatrix}.
     * \f]
     *
     * @param displacement Displacement \f$ \boldsymbol{d} \f$.
     * @return SingleModeMultiPeak<Float>
     */
    static SingleModeMultiPeak<Float> GenerateCoherent(const std::complex<Float>& displacement) {
        return GenerateSqueezed(0, 0, displacement);
    }

    /**
     * @brief Generate vacuum state.
     *
     * Wigner function is represented by mean
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} = \boldsymbol{0} \f]
     * and covariance matrix
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp} \\ \sigma_{xp} & \sigma_p^2
     * \end{pmatrix} = \begin{pmatrix} \hbar/2 & \\ & \hbar/2 \end{pmatrix}.
     * \f]
     *
     * @return SingleModeMultiPeak<Float>
     */
    static SingleModeMultiPeak<Float> GenerateVacuum() {
        return GenerateCoherent(std::complex<Float>{0, 0});
    }

    /**
     * @brief Generate cat state.
     *
     * Wigner function is represented by four peaks. Means are
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} \in \left\{ \sqrt{2
     * \hbar}\begin{pmatrix} \text{Re}(\alpha)
     * \\ \text{Im}(\alpha) \end{pmatrix}, -\sqrt{2\hbar}\begin{pmatrix}
     * \text{Re}(\alpha)
     * \\ \text{Im}(\alpha) \end{pmatrix}, \sqrt{2\hbar}\begin{pmatrix} i\text{Im}(\alpha)
     * \\ -i\text{Re}(\alpha) \end{pmatrix}, \sqrt{2\hbar}\begin{pmatrix} -i\text{Im}(\alpha)
     * \\ i\text{Re}(\alpha) \end{pmatrix} \right\}, \f] and the corresponding coefficients are
     * \f[
     * c \in \left\{ \mathcal{N}, \mathcal{N}, e^{-i \pi k-2|\alpha|^2} \mathcal{N}, e^{i \pi
     * k-2|\alpha|^2} \mathcal{N} \right\}, \f] where \f[
     * \mathcal{N}=\frac{1}{2\left[1+e^{-2|\alpha|^2} \cos
     * (\pi k)\right]}. \f]
     * The covariance matrix of each peak in the cat state is the same as that of the vacuum.
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp}
     * \\ \sigma_{xp} & \sigma_p^2 \end{pmatrix} = \begin{pmatrix} \hbar/2 & \\ & \hbar/2
     * \end{pmatrix} \f]
     *
     * @param displacement Displacement \f$ \alpha \f$.
     * @param k Parity \f$ k \f$.
     * @return SingleModeMultiPeak<Float>
     */
    static SingleModeMultiPeak<Float> GenerateCat(const std::complex<Float>& displacement,
                                                  const Float parity) {
        const Float exp_minus_2alpha_2 = std::exp(-2 * std::pow(std::abs(displacement), 2));
        const Float n =
            0.5 / (1.0 + exp_minus_2alpha_2 * std::cos(std::numbers::pi_v<Float> * parity));
        const auto c = std::complex<Float>{n, 0};
        const std::complex<Float> c_z =
            std::polar(exp_minus_2alpha_2 * n, -std::numbers::pi_v<Float> * parity);
        const auto hs = std::sqrt(2 * Hbar<Float>);
        const auto mu = Vector2C<Float>{hs * std::complex<Float>{displacement.real(), 0},
                                        hs * std::complex<Float>{displacement.imag(), 0}};
        const auto mu_z = Vector2C<Float>{hs * std::complex<Float>{0, displacement.imag()},
                                          hs * std::complex<Float>{0, -displacement.real()}};
        const auto mu_z_conj = Vector2C<Float>{std::conj(mu_z(0)), std::conj(mu_z(1))};
        const auto cov = (Hbar<Float> / 2) * Matrix2D<Float>::Identity();
        auto peak_p = SingleModeSinglePeak<Float>{c, mu, cov};
        auto peak_m = SingleModeSinglePeak<Float>{c, -mu, cov};
        auto peak_z = SingleModeSinglePeak<Float>{c_z, mu_z, cov};
        auto peak_z_conj = SingleModeSinglePeak<Float>{std::conj(c_z), mu_z_conj, cov};
        return SingleModeMultiPeak{{peak_p, peak_m, peak_z, peak_z_conj}};
    }
};

/**
 * @brief Gaussian parameters for a single peak in multiple modes.
 *
 * @tparam Float
 */
template <std::floating_point Float>
class MultiModeSinglePeak {
public:
    explicit MultiModeSinglePeak<Float>(const std::complex<Float>& coeff,
                                        const VectorC<Float>& mean, const MatrixD<Float>& cov,
                                        // If true, assume that cov is positive semi-definite.
                                        bool assume_cov_psd = true)
        : coeff_(coeff), mean_(mean), cov_(cov) {
        if (mean.size() % 2 != 0) {
            throw std::invalid_argument(
                "Argument mean must be a vector whose dimensionality is even");
        }
        if (cov.rows() != mean.size()) {
            throw std::invalid_argument(
                "The dimensions of argument cov must match those of argument mean");
        }
        if (!assume_cov_psd && !IsPositiveSemidefinite(cov)) {
            throw std::invalid_argument(
                "Invalid covariance matrix: it must be symmetric and positive semidefinite");
        }
        n_modes_ = static_cast<std::size_t>(mean.size()) / 2;
    }
    explicit MultiModeSinglePeak<Float>(const std::vector<SingleModeSinglePeak<Float>>& peaks) {
        if (peaks.size() > static_cast<std::size_t>(std::numeric_limits<long>::max())) {
            throw std::overflow_error("The size of argument peaks is too large");
        }
        n_modes_ = peaks.size();
        const auto n_modes_l = static_cast<long>(n_modes_);
        coeff_ = std::accumulate(
            std::ranges::views::transform(peaks, &SingleModeSinglePeak<Float>::coeff).begin(),
            std::ranges::views::transform(peaks, &SingleModeSinglePeak<Float>::coeff).end(),
            std::complex<Float>(1, 0), std::multiplies<>());
        mean_ = VectorC<Float>::Zero(n_modes_l * 2);
        cov_ = MatrixD<Float>::Zero(n_modes_l * 2, n_modes_l * 2);
        for (long mode{0}; const auto& p : peaks) {
            const auto p_idx{mode + n_modes_l};
            mean_(mode) = p.mean(0);
            mean_(p_idx) = p.mean(1);
            cov_(mode, mode) = p.cov(0, 0);
            cov_(mode, p_idx) = p.cov(0, 1);
            cov_(p_idx, mode) = p.cov(1, 0);
            cov_(p_idx, p_idx) = p.cov(1, 1);
            ++mode;
        }
    }

    MultiModeSinglePeak() = default;

    std::size_t NumModes() const { return n_modes_; }
    std::complex<Float> GetCoeff() const { return coeff_; }
    const VectorC<Float>& GetMean() const { return mean_; }
    const MatrixD<Float>& GetCov() const { return cov_; }
    std::complex<Float>& GetMutCoeff() { return coeff_; }
    VectorC<Float>& GetMutMean() { return mean_; }
    MatrixD<Float>& GetMutCov() { return cov_; }
    void SetCoeff(const std::complex<Float>& coeff) { coeff_ = coeff; }
    void SetMean(const VectorC<Float>& mean) {
        if (mean.size() != 2 * static_cast<long>(n_modes_)) {
            throw std::invalid_argument(
                "The size of argument mean must match the size of the vector to be modified");
        }
        mean_ = mean;
    }
    void SetCov(const MatrixD<Float>& cov) {
        if (cov.rows() != 2 * static_cast<long>(n_modes_) ||
            cov.cols() != 2 * static_cast<long>(n_modes_)) {
            throw std::invalid_argument(
                "The size of argument cov must match the size of the matrix to be modified");
        }
        cov_ = cov;
    }

    std::complex<Float> GetMeanX(const std::size_t mode) const {
        CheckModeValue(mode);
        return mean_(static_cast<long>(mode));
    }
    std::complex<Float> GetMeanP(const std::size_t mode) const {
        CheckModeValue(mode);
        return mean_(static_cast<long>(n_modes_ + mode));
    }
    Float GetCovXX(const std::size_t mode0, const std::size_t mode1) const {
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return cov_(static_cast<long>(mode0), static_cast<long>(mode1));
    }
    Float GetCovXP(const std::size_t mode0, const std::size_t mode1) const {
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return cov_(static_cast<long>(mode0), static_cast<long>(n_modes_ + mode1));
    }
    Float GetCovPX(const std::size_t mode0, const std::size_t mode1) const {
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return cov_(static_cast<long>(n_modes_ + mode0), static_cast<long>(mode1));
    }
    Float GetCovPP(const std::size_t mode0, const std::size_t mode1) const {
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return cov_(static_cast<long>(n_modes_ + mode0), static_cast<long>(n_modes_ + mode1));
    }

private:
    std::size_t n_modes_;
    std::complex<Float> coeff_;
    VectorC<Float> mean_;
    MatrixD<Float> cov_;

    void CheckModeValue([[maybe_unused]] const std::size_t mode) const {
#ifndef NDEBUG
        if (mode >= n_modes_) {
            throw std::out_of_range("Mode index must be smaller than the number of modes");
        }
#endif
    }
};

/**
 * @brief Gaussian parameters for multiple peaks in multiple modes.
 *
 * @tparam Float
 */
template <std::floating_point Float>
class State {
public:
    explicit State(const std::vector<SingleModeMultiPeak<Float>>& modes) {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::ConstructState>{};
#endif
        if (modes.size() > static_cast<std::size_t>(std::numeric_limits<long>::max())) {
            throw std::overflow_error("The size of argument peaks is too large");
        }
        n_modes_ = modes.size();
        // const auto sizes =
        //     modes | std::views::transform([](const auto& x) { return x.peaks.size(); });
        auto sizes = std::vector<std::size_t>{};
        std::transform(modes.begin(), modes.end(), std::back_inserter(sizes),
                       [](const auto& x) { return x.peaks.size(); });
        const std::size_t n_peaks =
            std::accumulate(sizes.begin(), sizes.end(), 1u, std::multiplies<>());
        peaks_.reserve(n_peaks);
        auto current = std::vector<SingleModeSinglePeak<Float>>{};
        current.reserve(n_modes_);
        auto all_1mode_peak_combs = std::vector<std::vector<SingleModeSinglePeak<Float>>>(n_peaks);
        auto direct_product = [&modes, &current, &all_1mode_peak_combs](
                                  auto self, std::size_t& i_peak, const std::size_t depth = 0) {
            if (depth == modes.size()) {
                all_1mode_peak_combs[i_peak] = current;
                ++i_peak;
                return;
            }
            for (const auto& peak : modes[depth].peaks) {
                current.push_back(peak);
                self(self, i_peak, depth + 1);
                current.pop_back();
            }
        };

        auto i_peak = std::size_t{0};
        direct_product(direct_product, i_peak);
        ParallelConstructPeaks(all_1mode_peak_combs);
    }
    explicit State(const std::vector<MultiModeSinglePeak<Float>>& peaks) : peaks_(peaks) {
        n_modes_ = (peaks.empty() ? 0 : peaks[0].NumModes());
    }

    State(const State& other)
        : n_modes_(other.n_modes_), peaks_(other.peaks_), engine_(other.engine_),
          shot_(other.shot_) {}

    State& operator=(const State& other) {
        if (this != &other) {
            n_modes_ = other.n_modes_;
            peaks_ = other.peaks_;
            engine_ = other.engine_;
            shot_ = other.shot_;
        }
        return *this;
    }

    State(State&& other) noexcept
        : n_modes_(other.n_modes_), peaks_(std::move(other.peaks_)),
          engine_(std::move(other.engine_)), shot_(other.shot_) {}

    State& operator=(State&& other) noexcept {
        if (this != &other) {
            n_modes_ = other.n_modes_;
            peaks_ = std::move(other.peaks_);
            engine_ = std::move(other.engine_);
            shot_ = other.shot_;
        }
        return *this;
    }

    std::size_t NumModes() const { return n_modes_; }
    const std::vector<MultiModeSinglePeak<Float>>& GetPeaks() const { return peaks_; }

    /**
     * @brief Get the number of peaks.
     *
     * @return std::size_t
     */
    std::size_t NumPeaks() const { return peaks_.size(); }

    /**
     * @brief Get the L1 norm of the coefficients.
     *
     * @return Float
     */
    Float GetExtent() const {
        return std::accumulate(
            peaks_.begin(), peaks_.end(), Float{0},
            [](Float acc, const auto& x) { return acc + std::abs(x.GetCoeff()); });
    }

    std::complex<Float> GetCoeff(const std::size_t peak) const {  // TODO Add test
        CheckPeakValue(peak);
        return peaks_[peak].GetCoeff();
    }
    const VectorC<Float>& GetMean(const std::size_t peak) const {  // TODO Add test
        CheckPeakValue(peak);
        return peaks_[peak].GetMean();
    }
    const MatrixD<Float>& GetCov(const std::size_t peak) const {  // TODO Add test
        CheckPeakValue(peak);
        return peaks_[peak].GetCov();
    }
    std::complex<Float>& GetMutCoeff(const std::size_t peak) {  // TODO Add test
        CheckPeakValue(peak);
        return peaks_[peak].GetMutCoeff();
    }
    VectorC<Float>& GetMutMean(const std::size_t peak) {  // TODO Add test
        CheckPeakValue(peak);
        return peaks_[peak].GetMutMean();
    }
    MatrixD<Float>& GetMutCov(const std::size_t peak) {  // TODO Add test
        CheckPeakValue(peak);
        return peaks_[peak].GetMutCov();
    }
    std::complex<Float> GetMeanX(const std::size_t peak,
                                 const std::size_t mode) const {  // TODO Add test
        CheckPeakValue(peak);
        CheckModeValue(mode);
        return peaks_[peak].GetMeanX(mode);
    }
    std::complex<Float> GetMeanP(const std::size_t peak,
                                 const std::size_t mode) const {  // TODO Add test
        CheckPeakValue(peak);
        CheckModeValue(mode);
        return peaks_[peak].GetMeanP(mode);
    }
    Float GetCovXX(const std::size_t peak, const std::size_t mode0,
                   const std::size_t mode1) const {  // TODO Add test
        CheckPeakValue(peak);
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return peaks_[peak].GetCovXX(mode0, mode1);
    }
    Float GetCovXP(const std::size_t peak, const std::size_t mode0,
                   const std::size_t mode1) const {  // TODO Add test
        CheckPeakValue(peak);
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return peaks_[peak].GetCovXP(mode0, mode1);
    }
    Float GetCovPX(const std::size_t peak, const std::size_t mode0,
                   const std::size_t mode1) const {  // TODO Add test
        CheckPeakValue(peak);
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return peaks_[peak].GetCovPX(mode0, mode1);
    }
    Float GetCovPP(const std::size_t peak, const std::size_t mode0,
                   const std::size_t mode1) const {  // TODO Add test
        CheckPeakValue(peak);
        CheckModeValue(mode0);
        CheckModeValue(mode1);
        return peaks_[peak].GetCovPP(mode0, mode1);
    }

    /**
     * @brief Generate single mode vacuum state.
     *
     * Wigner function is represented by mean
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} = \boldsymbol{0} \f]
     * and covariance matrix
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp} \\ \sigma_{xp} & \sigma_p^2
     * \end{pmatrix} = \begin{pmatrix} \hbar/2 & \\ & \hbar/2 \end{pmatrix}.
     * \f]
     *
     * @return State<Float>
     */
    static State<Float> GenerateVacuum() {
        return State<Float>{{SingleModeMultiPeak<Float>::GenerateVacuum()}};
    }

    /**
     * @brief Generate single mode coherent state.
     *
     * Wigner function is represented by mean
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} = \boldsymbol{d} \f]
     * and covariance matrix
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp} \\ \sigma_{xp} & \sigma_p^2
     * \end{pmatrix} = \begin{pmatrix} \hbar/2 & \\ & \hbar/2 \end{pmatrix}.
     * \f]
     *
     * @param displacement Displacement \f$ \boldsymbol{d} \f$.
     * @return State<Float>
     */
    static State<Float> GenerateCoherent(const std::complex<Float>& displacement) {
        return State<Float>{{SingleModeMultiPeak<Float>::GenerateCoherent(displacement)}};
    }

    /**
     * @brief Generate single mode squeezed state.
     *
     * Wigner function is represented by mean
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} = \boldsymbol{d} \f]
     * and covariance matrix
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp} \\ \sigma_{xp} & \sigma_p^2
     * \end{pmatrix} = RS \begin{pmatrix} \hbar/2 & \\ & \hbar/2 \end{pmatrix} S^\text{T}R^\text{T},
     * \f]
     * where
     * \f[
     * S = \begin{pmatrix} \frac{1}{\sqrt{s\text{ dB}}} & \\ & \sqrt{s\text{ dB}} \end{pmatrix}
     * = \begin{pmatrix} 10^{-\frac{s}{20}} & \\ & 10^{\frac{s}{20}} \end{pmatrix},
     * \f]
     * \f[
     * R = \begin{pmatrix} \cos \phi & -\sin \phi \\ \sin \phi & \cos \phi \end{pmatrix}.
     * \f]
     *
     * @param squeezing_level Squeezing level in dB \f$ s \f$.
     * @param phi Squeezing angle \f$ \phi \f$.
     * @param displacement Displacement \f$ \boldsymbol{d} \f$.
     * @return State<Float>
     */
    static State<Float> GenerateSqueezed(const Float squeezing_level, const Float phi,
                                         const std::complex<Float>& displacement) {
        return State<Float>{
            {SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level, phi, displacement)}};
    }

    /**
     * @brief Generate single mode cat state.
     *
     * Wigner function is represented by four peaks. Means are
     * \f[
     * \boldsymbol{\mu} = \begin{pmatrix} \mu_x \\ \mu_y \end{pmatrix} \in \left\{ \sqrt{2
     * \hbar}\begin{pmatrix} \text{Re}(\alpha)
     * \\ \text{Im}(\alpha) \end{pmatrix}, -\sqrt{2\hbar}\begin{pmatrix}
     * \text{Re}(\alpha)
     * \\ \text{Im}(\alpha) \end{pmatrix}, \sqrt{2\hbar}\begin{pmatrix} i\text{Im}(\alpha)
     * \\ -i\text{Re}(\alpha) \end{pmatrix}, \sqrt{2\hbar}\begin{pmatrix} -i\text{Im}(\alpha)
     * \\ i\text{Re}(\alpha) \end{pmatrix} \right\}, \f] and the corresponding coefficients are
     * \f[
     * c \in \left\{ \mathcal{N}, \mathcal{N}, e^{-i \pi k-2|\alpha|^2} \mathcal{N}, e^{i \pi
     * k-2|\alpha|^2} \mathcal{N} \right\}, \f] where \f[
     * \mathcal{N}=\frac{1}{2\left[1+e^{-2|\alpha|^2} \cos
     * (\pi k)\right]}. \f]
     * The covariance matrix of each peak in the cat state is the same as that of the vacuum.
     * \f[
     * \boldsymbol{\Sigma} = \begin{pmatrix} \sigma_x^2 & \sigma_{xp}
     * \\ \sigma_{xp} & \sigma_p^2 \end{pmatrix} = \begin{pmatrix} \hbar/2 & \\ & \hbar/2
     * \end{pmatrix} \f]
     *
     * @param displacement Displacement \f$ \alpha \f$.
     * @param k Parity \f$ k \f$.
     * @return State<Float>
     */
    static State<Float> GenerateCat(const std::complex<Float>& displacement, const Float k) {
        return State<Float>{{SingleModeMultiPeak<Float>::GenerateCat(displacement, k)}};
    }

    std::complex<Float> WignerFuncValue(const std::size_t mode, const Float x,
                                        const Float p) const {
        auto sum = std::complex<Float>{};
        for (std::size_t m = 0; m < NumPeaks(); ++m) {
            const auto mu = Vector2C<Float>{GetMeanX(m, mode), GetMeanP(m, mode)};
            auto cov = Matrix2D<Float>{};
            cov << GetCovXX(m, mode, mode), GetCovXP(m, mode, mode), GetCovPX(m, mode, mode),
                GetCovPP(m, mode, mode);
            const auto gaussian = General2DGaussianPdf(mu, cov);
            sum += GetCoeff(m) * gaussian(x, p);
        }
        return sum;
    }

    void DumpToJson(const std::filesystem::path& json_path, const int indent = 4) const {
        if (json_path.extension() != ".json") {
            throw std::invalid_argument("The file does not have a .json extension: " +
                                        json_path.string());
        }

        auto json = nlohmann::ordered_json{};
        json["n_modes"] = n_modes_;
        json["n_peaks"] = NumPeaks();
        json["peaks"] = nlohmann::ordered_json::array();

        for (const auto& p : peaks_) {
            auto peak_json = nlohmann::ordered_json{};

            {
                auto complex = nlohmann::ordered_json{};
                complex["real"] = p.GetCoeff().real();
                complex["imag"] = p.GetCoeff().imag();
                peak_json["coeff"] = complex;
            }
            {
                auto mean = nlohmann::ordered_json::array();
                const long size = static_cast<long>(p.GetMean().size());
                for (long i = 0; i < size; ++i) {
                    auto complex = nlohmann::ordered_json{};
                    complex["real"] = p.GetMean()(i).real();
                    complex["imag"] = p.GetMean()(i).imag();
                    mean.push_back(complex);
                }
                peak_json["mean"] = mean;
            }
            {
                auto cov = nlohmann::ordered_json::array();
                const long size = static_cast<long>(p.GetCov().rows());
                for (long i = 0; i < size; ++i) {
                    for (long j = 0; j < size; ++j) { cov.push_back(p.GetCov()(i, j)); }
                }
                peak_json["cov"] = cov;
            }
            json["peaks"].push_back(peak_json);
        }
        auto ofs = std::ofstream{json_path};
        ofs << json.dump(indent);
        ofs.close();
    }

    void SetSeed(const std::uint64_t seed) { engine_.seed(seed); }
    void SetShot(const std::size_t shot) { shot_ = shot; }
    std::mt19937_64& GetEngine() const { return engine_; }
    std::size_t GetShot() const { return shot_; }

private:
    std::size_t n_modes_;
    std::vector<MultiModeSinglePeak<Float>> peaks_;
    mutable std::mt19937_64 engine_;
    std::size_t shot_ = 0;

    void ParallelConstructPeaks(
        const std::vector<std::vector<SingleModeSinglePeak<Float>>>& all_1mode_peak_combs) {
        const auto num_peaks = all_1mode_peak_combs.size();
        this->peaks_.resize(num_peaks);

        const auto hardware_concurrency = std::thread::hardware_concurrency();
        const auto max_threads =
            static_cast<std::size_t>(hardware_concurrency == 0 ? 1 : hardware_concurrency);

        const auto num_threads = std::min(num_peaks, max_threads);
        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        const auto peaks_per_thread = num_peaks / num_threads;
        const auto num_additional_peaks = num_peaks % num_threads;

        // Divide the peaks into `num_threads` chunks and construct them in parallel.
        // The first `num_additional_peaks` threads construct `peaks_per_thread` + 1 peaks.
        // The remaining threads construct `peaks_per_thread` peaks.
        auto peak_start = std::size_t{0};
        for (auto i = std::size_t{0}; i < num_threads; ++i) {
            const auto num_local_peaks =
                static_cast<std::size_t>(peaks_per_thread + (i < num_additional_peaks ? 1 : 0));
            const auto peak_end = peak_start + num_local_peaks;
            threads.emplace_back([this, &all_1mode_peak_combs, peak_start, peak_end]() {
                for (auto j = peak_start; j < peak_end; ++j) {
                    this->peaks_[j] = MultiModeSinglePeak<Float>(all_1mode_peak_combs[j]);
                }
            });
            peak_start = peak_end;
        }

        for (auto& thread : threads) { thread.join(); }
    }

    void CheckModeValue([[maybe_unused]] const std::size_t mode) const {
#ifndef NDEBUG
        if (mode >= n_modes_) {
            throw std::out_of_range("Mode index must be smaller than the number of modes");
        }
#endif
    }
    void CheckPeakValue([[maybe_unused]] const std::size_t peak) const {
#ifndef NDEBUG
        if (peak >= peaks_.size()) {
            throw std::out_of_range("Peak index must be smaller than the number of peaks");
        }
#endif
    }
};
}  // namespace bosim
