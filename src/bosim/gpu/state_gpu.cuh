#pragma once

#include <Eigen/Dense>
#include <cuda.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <concepts>
#include <random>
#include <thrust/complex.h>
#include <thrust/device_vector.h>
#include <thrust/execution_policy.h>
#include <thrust/gather.h>
#include <thrust/host_vector.h>
#include <thrust/iterator/constant_iterator.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/random/normal_distribution.h>
#include <thrust/transform.h>
#include <thrust/transform_reduce.h>
#include <vector>

#include "bosim/base/timer.h"
#include "bosim/exception.h"
#include "bosim/operation.h"
#include "bosim/state.h"

namespace bosim::gpu {

namespace internal {

template <EigenMatrix Matrix>
auto MatrixToVector(const Matrix& from) {
    auto result = std::vector<typename Matrix::Scalar>(static_cast<std::size_t>(from.size()));
    for (long i = 0; i < from.rows(); ++i) {
        for (long j = 0; j < from.cols(); ++j) {
            const auto index = static_cast<std::size_t>(i * from.cols() + j);
            result[index] = from(i, j);
        }
    }
    return result;
}
template <std::floating_point Float>
auto VectorToMatrixD(const std::vector<Float>& from, const std::size_t rows,
                     const std::size_t cols) {
    auto result = MatrixD<Float>(rows, cols);
    for (std::size_t i = 0; i < rows; ++i) {
        for (std::size_t j = 0; j < cols; ++j) {
            result(static_cast<long>(i), static_cast<long>(j)) = from[i * cols + j];
        }
    }
    return result;
}

template <std::ranges::sized_range Range>
constexpr std::size_t GetBytes(const Range& range) {
    return range.size() * sizeof(typename Range::value_type);
}

// MARK: GPUTempBuffers
template <std::floating_point Float>
class GPUTempBuffers {
    template <typename T>
    using PinnedAllocator =
        thrust::mr::stateless_resource_allocator<T, thrust::universal_host_pinned_memory_resource>;

    using HostIndexVector = thrust::host_vector<std::size_t, PinnedAllocator<std::size_t>>;
    using HostFloatVector = thrust::host_vector<Float, PinnedAllocator<Float>>;
    using IndexVector = thrust::device_vector<std::size_t>;
    using FloatVector = thrust::device_vector<Float>;
    using ComplexVector = thrust::device_vector<thrust::complex<Float>>;

public:
    GPUTempBuffers() = default;
    GPUTempBuffers(const size_t n_modes, const size_t n_peaks)
        :  // Displacement
          displacement_indices_(n_peaks * 2), displacement_values_(n_peaks * 2),
          // Update state by unary or binary operation (operated indices: 2 or 4)
          h_operated_indices_2_(2), h_operated_indices_4_(4), h_matrix_2_(2 * 2),
          h_matrix_4_(4 * 4),  // host
          operated_indices_2_(2), operated_indices_4_(4), matrix_2_(2 * 2),
          matrix_4_(4 * 4),  // device
          // Mu update (operated indices: 2 or 4)
          mu_operated_indices_2_(n_peaks * 2), mu_operated_indices_4_(n_peaks * 4),
          mu_operated_values_2_(n_peaks * 2), mu_operated_values_4_(n_peaks * 4),
          // Sigma off-diagonal (operated indices: 2 or 4)
          sigma_off_diagonal_indices_2_(n_peaks * 2 * (2 * n_modes)),
          sigma_off_diagonal_indices_4_(n_peaks * 4 * (2 * n_modes)),
          sigma_off_diagonal_values_2_(n_peaks * 2 * (2 * n_modes)),
          sigma_off_diagonal_values_4_(n_peaks * 4 * (2 * n_modes)),
          // Sigma operated (2x2 or 4x4 matrix indices)
          sigma_operated_indices_2_(n_peaks * 2 * 2), sigma_operated_indices_4_(n_peaks * 4 * 4),
          sigma_operated_values_2_(n_peaks * 2 * 2), sigma_operated_values_4_(n_peaks * 4 * 4),
          // Sampling
          peak_types_(n_peaks), proposal_peak_coeffs_(n_peaks), sampling_mu_indices_(n_peaks),
          sampling_mu_values_(n_peaks), sampling_sigma_indices_(n_peaks),
          sampling_sigma_values_(n_peaks),
          // Post-selection
          cov_copy_indices_(n_peaks * (2 * n_modes)), cov_copy_values_(n_peaks * (2 * n_modes)),
          mean_b_indices_(n_peaks * 2), cov_ab_indices_(n_peaks * (2 * n_modes) * 4),
          cov_b_indices_(n_peaks * 2), operated_mean_indices_(n_peaks * 2),
          operated_cov_indices_(n_peaks * 4), mode_xp_indices_(2), squeezed_covs_(n_peaks * 2 * 2),
          squeezed_cov_(2 * 2) {}

    // DisplacementGPU
    IndexVector& DisplacementIndices() { return displacement_indices_; }
    ComplexVector& DisplacementValues() { return displacement_values_; }

    // UpdateStateGPU
    HostIndexVector& HostOperatedIndices(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? h_operated_indices_2_ : h_operated_indices_4_;
    }
    HostFloatVector& HostMatrix(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? h_matrix_2_ : h_matrix_4_;
    }
    IndexVector& OperatedIndices(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? operated_indices_2_ : operated_indices_4_;
    }
    FloatVector& Matrix(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? matrix_2_ : matrix_4_;
    }
    IndexVector& MuOperatedIndices(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? mu_operated_indices_2_ : mu_operated_indices_4_;
    }
    ComplexVector& MuOperatedValues(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? mu_operated_values_2_ : mu_operated_values_4_;
    }
    IndexVector& SigmaOffDiagonalIndices(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? sigma_off_diagonal_indices_2_
                                          : sigma_off_diagonal_indices_4_;
    }
    FloatVector& SigmaOffDiagonalValues(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? sigma_off_diagonal_values_2_
                                          : sigma_off_diagonal_values_4_;
    }
    IndexVector& SigmaOperatedIndices(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? sigma_operated_indices_2_ : sigma_operated_indices_4_;
    }
    FloatVector& SigmaOperatedValues(const std::size_t operated_indices_size) {
        return operated_indices_size == 2 ? sigma_operated_values_2_ : sigma_operated_values_4_;
    }

    // Sampling
    thrust::device_vector<std::uint8_t>& PeakTypes() { return peak_types_; }
    FloatVector& ProposalPeakCoeffs() { return proposal_peak_coeffs_; }
    IndexVector& SamplingMuIndices() { return sampling_mu_indices_; }
    ComplexVector& SamplingMuValues() { return sampling_mu_values_; }
    IndexVector& SamplingSigmaIndices() { return sampling_sigma_indices_; }
    FloatVector& SamplingSigmaValues() { return sampling_sigma_values_; }

    // PostSelectBySamplingResult
    IndexVector& CovCopyIndices() { return cov_copy_indices_; }
    FloatVector& CovCopyValues() { return cov_copy_values_; }
    IndexVector& MeanBIndices() { return mean_b_indices_; }
    IndexVector& CovABIndices() { return cov_ab_indices_; }
    IndexVector& CovBIndices() { return cov_b_indices_; }

    // ReplaceBySqueezedStateGPU
    IndexVector& OperatedMeanIndices() { return operated_mean_indices_; }
    IndexVector& OperatedCovIndices() { return operated_cov_indices_; }
    IndexVector& ModeXPIndices() { return mode_xp_indices_; }
    FloatVector& SqueezedCovs() { return squeezed_covs_; }
    FloatVector& SqueezedCov() { return squeezed_cov_; }

    // total bytes of used memories
    std::size_t Bytes() const {
        if (displacement_indices_.empty()) return 0;
        return GetBytes(displacement_indices_) + GetBytes(displacement_values_) +
               GetBytes(operated_indices_2_) + GetBytes(operated_indices_4_) +  // operated indices
               GetBytes(matrix_2_) + GetBytes(matrix_4_) +                      // matrix
               GetBytes(mu_operated_indices_2_) + GetBytes(mu_operated_indices_4_) +
               GetBytes(mu_operated_values_2_) + GetBytes(mu_operated_values_4_) +
               GetBytes(sigma_off_diagonal_indices_2_) + GetBytes(sigma_off_diagonal_indices_4_) +
               GetBytes(sigma_off_diagonal_values_2_) + GetBytes(sigma_off_diagonal_values_4_) +
               GetBytes(sigma_operated_indices_2_) + GetBytes(sigma_operated_indices_4_) +
               GetBytes(sigma_operated_values_2_) + GetBytes(sigma_operated_values_4_) +
               GetBytes(peak_types_) + GetBytes(proposal_peak_coeffs_) +
               GetBytes(sampling_mu_indices_) + GetBytes(sampling_mu_values_) +
               GetBytes(sampling_sigma_indices_) + GetBytes(sampling_sigma_values_) +
               GetBytes(cov_copy_indices_) + GetBytes(cov_copy_values_) +
               GetBytes(mean_b_indices_) + GetBytes(cov_ab_indices_) + GetBytes(cov_b_indices_) +
               GetBytes(operated_mean_indices_) + GetBytes(operated_cov_indices_) +
               GetBytes(mode_xp_indices_) + GetBytes(squeezed_covs_) + GetBytes(squeezed_cov_);
    }

private:
    // DisplacementGPU
    IndexVector displacement_indices_;
    ComplexVector displacement_values_;

    // UpdateStateGPU
    HostIndexVector h_operated_indices_2_;
    HostIndexVector h_operated_indices_4_;
    HostFloatVector h_matrix_2_;
    HostFloatVector h_matrix_4_;
    IndexVector operated_indices_2_;
    IndexVector operated_indices_4_;
    FloatVector matrix_2_;
    FloatVector matrix_4_;
    IndexVector mu_operated_indices_2_;
    IndexVector mu_operated_indices_4_;
    ComplexVector mu_operated_values_2_;
    ComplexVector mu_operated_values_4_;
    IndexVector sigma_off_diagonal_indices_2_;
    IndexVector sigma_off_diagonal_indices_4_;
    FloatVector sigma_off_diagonal_values_2_;
    FloatVector sigma_off_diagonal_values_4_;
    IndexVector sigma_operated_indices_2_;
    IndexVector sigma_operated_indices_4_;
    FloatVector sigma_operated_values_2_;
    FloatVector sigma_operated_values_4_;

    // Sampling
    thrust::device_vector<std::uint8_t> peak_types_;
    FloatVector proposal_peak_coeffs_;
    IndexVector sampling_mu_indices_;
    ComplexVector sampling_mu_values_;
    IndexVector sampling_sigma_indices_;
    FloatVector sampling_sigma_values_;

    // PostSelectBySamplingResult
    IndexVector cov_copy_indices_;
    FloatVector cov_copy_values_;
    IndexVector mean_b_indices_;
    IndexVector cov_ab_indices_;
    IndexVector cov_b_indices_;

    // ReplaceBySqueezedStateGPU
    IndexVector operated_mean_indices_;
    IndexVector operated_cov_indices_;
    IndexVector mode_xp_indices_;
    FloatVector squeezed_covs_;
    FloatVector squeezed_cov_;
};

// MARK: UpdateState
template <std::floating_point Float>
struct ComputeMuOperatedFunctor {
    const thrust::complex<Float>* means_ptr;
    const Float* matrix_ptr;
    const std::size_t* operated_indices_ptr;
    const std::size_t operated_indices_size;
    const std::size_t step_mean;

    __host__ __device__ thrust::complex<Float> operator()(const std::size_t index) const {
        const auto peak_idx = index / operated_indices_size;
        const auto a = index % operated_indices_size;

        auto tmp = thrust::complex<Float>();
        for (std::size_t b = 0; b < operated_indices_size; ++b) {
            const auto mu = means_ptr[peak_idx * step_mean + operated_indices_ptr[b]];
            const auto coeff = matrix_ptr[a * operated_indices_size + b];
            tmp += coeff * mu;
        }
        return tmp;
    }
};
struct CalculateMuOperatedMappingFunctor {
    const std::size_t* operated_indices_ptr;
    const std::size_t operated_indices_size;
    const std::size_t step_mean;

    __host__ __device__ std::size_t operator()(const std::size_t index) const {
        const auto peak_idx = index / operated_indices_size;
        const auto a = index % operated_indices_size;

        return peak_idx * step_mean + operated_indices_ptr[a];
    }
};
template <std::floating_point Float>
struct ComputeSigmaOperatedFunctor {
    const Float* covs_ptr;
    const Float* matrix_ptr;
    const std::size_t* operated_indices_ptr;
    const std::size_t operated_indices_size;
    const std::size_t dim;
    const std::size_t step_cov;

    __host__ __device__ Float operator()(const std::size_t index) const {
        const auto peak_idx = index / (operated_indices_size * operated_indices_size);
        const auto ab = index % (operated_indices_size * operated_indices_size);
        const auto a = ab / operated_indices_size;
        const auto b = ab % operated_indices_size;

        auto tmp = Float();
        for (std::size_t c = 0; c < operated_indices_size; ++c) {
            const auto k = operated_indices_ptr[c];
            for (std::size_t d = 0; d < operated_indices_size; ++d) {
                const auto l = operated_indices_ptr[d];
                const auto kl = peak_idx * step_cov + k * dim + l;

                tmp += matrix_ptr[a * operated_indices_size + c] * covs_ptr[kl] *
                       matrix_ptr[b * operated_indices_size + d];
            }
        }
        return tmp;
    }
};
struct CalculateSigmaOperatedMappingFunctor {
    const std::size_t* operated_indices_ptr;
    const std::size_t operated_indices_size;
    const std::size_t dim;
    const std::size_t step_cov;

    __host__ __device__ std::size_t operator()(const std::size_t index) const {
        const auto peak_idx = index / (operated_indices_size * operated_indices_size);
        const auto ab = index % (operated_indices_size * operated_indices_size);
        const auto a = ab / operated_indices_size;
        const auto b = ab % operated_indices_size;

        return peak_idx * step_cov + operated_indices_ptr[a] * dim + operated_indices_ptr[b];
    }
};
template <std::floating_point Float>
struct ComputeSigmaOffDiagonalFunctor {
    const Float* covs_ptr;
    const Float* matrix_ptr;
    const std::size_t* operated_indices_ptr;
    const std::size_t operated_indices_size;
    const std::size_t dim;
    const std::size_t step_cov;

    __host__ __device__ Float operator()(const std::size_t index) const {
        const auto peak_idx = index / (operated_indices_size * dim);
        const auto aj = index % (operated_indices_size * dim);
        const auto a = aj / dim;
        const auto j = aj % dim;

        auto tmp = Float();
        for (std::size_t c = 0; c < operated_indices_size; ++c) {
            const auto k = operated_indices_ptr[c];
            const auto jk = peak_idx * step_cov + j * dim + k;

            tmp += matrix_ptr[a * operated_indices_size + c] * covs_ptr[jk];
        }
        return tmp;
    }
};
template <bool Order>
struct CalculateSigmaOffDiagonalMappingFunctor {
    const std::size_t* operated_indices_ptr;
    const std::size_t operated_indices_size;
    const std::size_t dim;
    const std::size_t step_cov;

    __host__ __device__ std::size_t operator()(const std::size_t index) const {
        const auto peak_idx = index / (operated_indices_size * dim);
        const auto aj = index % (operated_indices_size * dim);
        const auto a = aj / dim;
        const auto j = aj % dim;

        if constexpr (Order) {
            return peak_idx * step_cov + operated_indices_ptr[a] * dim + j;  // ij
        }
        return peak_idx * step_cov + j * dim + operated_indices_ptr[a];  // ji
    }
};

template <std::floating_point Float>
struct CalculateSqueezedCovFunctor {
    const Float* squeezed_cov_ptr;
    const std::size_t squeezed_cov_size;

    __host__ __device__ Float operator()(const std::size_t index) const {
        return squeezed_cov_ptr[index % squeezed_cov_size];
    }
};

// clang-format off
static const std::uint8_t CoeffReal = 0b00'000'001u;
static const std::uint8_t CoeffNeg  = 0b00'000'010u;
static const std::uint8_t MeanReal  = 0b00'000'100u;
// clang-format on

inline bool __host__ __device__ IsCoeffReal(std::uint8_t peak_type) {
    return 0 != (peak_type & CoeffReal);
}
inline bool __host__ __device__ IsCoeffNeg(std::uint8_t peak_type) {
    return 0 != (peak_type & CoeffNeg);
}
inline bool __host__ __device__ IsMeanReal(std::uint8_t peak_type) {
    return 0 != (peak_type & MeanReal);
}

template <std::floating_point Float>
inline bool __device__ IsReal(thrust::complex<Float> value) {
    return std::abs(value.imag()) < Eps<Float>;
}

template <std::floating_point Float>
struct SamplingResult {
    Float measured_x;                                                  //!< measured value.
    thrust::device_vector<thrust::complex<Float>> original_peak_vals;  //!< PDF value of each peak.
    Float original_pdf_val;  //!< sum of original_peak_vals.
};

template <std::floating_point Float>
thrust::complex<Float> __device__ General1DGaussianPdf(const Float x,
                                                       const thrust::complex<Float> mu,
                                                       const Float sigma) {
    const thrust::complex<Float> exponent =
        -Float{0.5} * thrust::pow((thrust::complex<Float>{x, 0} - mu) / sigma, 2);
    return (1 / (sigma * std::sqrt(2 * std::numbers::pi_v<Float>))) * thrust::exp(exponent);
}
/**
 * @brief 'boost::math::pdf' as a device function
 *
 * @tparam Float
 * @param x x value
 * @param mu mean
 * @param sigma standard_deviation
 * @return Float
 */
template <std::floating_point Float>
Float __device__ Pdf(const Float x, const Float mu, const Float sigma) {
    Float exponent = x - mu;
    exponent *= -exponent;
    exponent /= 2 * sigma * sigma;
    return std::exp(exponent) / (sigma * std::sqrt(2 * std::numbers::pi_v<Float>));
}

// MARK: Sampling
template <std::floating_point Float>
struct CalcPeakTypesFunctor {
    const thrust::complex<Float>* coeffs_ptr;
    const thrust::complex<Float>* means_ptr;
    const std::size_t x_idx;
    const std::size_t p_idx;
    const std::size_t step_mean;

    __host__ __device__ std::uint8_t operator()(const std::size_t peak_idx) const {
        const auto coeff = coeffs_ptr[peak_idx];
        const auto mean_x = means_ptr[peak_idx * step_mean + x_idx];
        const auto mean_p = means_ptr[peak_idx * step_mean + p_idx];

        std::uint8_t ret = 0;
        if (IsReal(coeff)) {
            ret |= CoeffReal;
            if (coeff.real() < 0) { ret |= CoeffNeg; }
        }
        if (IsReal(mean_x) && IsReal(mean_p)) { ret |= MeanReal; }
        return ret;
    }
};
template <std::floating_point Float>
struct CalcProposalPeakCoeffsFunctor {
    const thrust::complex<Float>* coeffs_ptr;
    const thrust::complex<Float>* means_ptr;
    const Float* covs_ptr;
    const std::size_t mu_idx;
    const std::size_t sigma_idx;
    const std::size_t step_mean;
    const std::size_t step_cov;

    __host__ __device__ Float operator()(const std::size_t peak_idx,
                                         const std::uint8_t peak_type) const {
        const auto coeff = coeffs_ptr[peak_idx];

        Float ret = 0;
        if (IsCoeffNeg(peak_type) && IsMeanReal(peak_type)) {
            ret = 0;
        } else if (IsMeanReal(peak_type)) {
            ret = thrust::abs(coeff);
        } else {
            const auto mu = means_ptr[peak_idx * step_mean + mu_idx];
            const auto sigma = covs_ptr[peak_idx * step_cov + sigma_idx];
            const Float exponent = mu.imag() * mu.imag() / (2 * sigma);
            ret = thrust::abs(coeff) * std::exp(exponent);
        }
        return ret;
    }
};
template <std::floating_point Float>
struct CalcProposalValueFunctor {
    using T = thrust::tuple<std::size_t, std::uint8_t, Float>;

    const thrust::complex<Float>* means_ptr;
    const Float* covs_ptr;
    const Float x;
    const std::size_t mu_idx;
    const std::size_t sigma_idx;
    const std::size_t step_mean;
    const std::size_t step_cov;

    __host__ __device__ Float operator()(const T& tuple) const {
        const auto peak_idx = thrust::get<0>(tuple);
        const auto peak_type = thrust::get<1>(tuple);
        const auto proposal_peak_coeff = thrust::get<2>(tuple);
        if (IsCoeffNeg(peak_type) && IsMeanReal(peak_type)) return 0;

        const auto mu = means_ptr[peak_idx * step_mean + mu_idx];
        const auto sigma = covs_ptr[peak_idx * step_cov + sigma_idx];
        return proposal_peak_coeff * Pdf<Float>(x, mu.real(), std::sqrt(sigma));
    }
};
template <std::floating_point Float>
struct CalcOriginalPdfFunctor {
    const thrust::complex<Float>* coeffs_ptr;
    const thrust::complex<Float>* means_ptr;
    const Float* covs_ptr;
    const Float x;
    const std::size_t mu_idx;
    const std::size_t sigma_idx;
    const std::size_t step_mean;
    const std::size_t step_cov;

    __host__ __device__ thrust::complex<Float> operator()(const std::size_t peak_idx) const {
        const auto coeff = coeffs_ptr[peak_idx];
        const auto mu = means_ptr[peak_idx * step_mean + mu_idx];
        const auto sigma = std::sqrt(covs_ptr[peak_idx * step_cov + sigma_idx]);
        return coeff * General1DGaussianPdf<Float>(x, mu, sigma);
    }
};

// MARK: PostSelectBySamplingResult
template <std::floating_point Float>
struct UpdateMeanAFunctor {
    const Float* covs_ptr;
    const Float* cov_copy_ptr;
    const thrust::complex<Float>* means_ptr;
    const thrust::complex<Float> measured_x;
    const std::size_t dim;
    const std::size_t x_idx;
    const std::size_t p_idx;
    const std::size_t step_mean;
    const std::size_t step_cov;

    __host__ __device__ thrust::complex<Float> operator()(
        const std::size_t index, const thrust::complex<Float> original) const {
        const auto peak_idx = index / step_mean;
        const auto i = index % step_mean;
        if (i == x_idx or i == p_idx) return original;

        const auto mu = means_ptr[peak_idx * step_mean + x_idx];
        const auto sigma = covs_ptr[peak_idx * step_cov + x_idx * dim + x_idx];
        return original + (measured_x - mu) / sigma * cov_copy_ptr[index];
    }
};
template <std::floating_point Float>
struct UpdateCovAFunctor {
    const Float* cov_copy_ptr;
    const std::size_t dim;
    const std::size_t x_idx;
    const std::size_t p_idx;
    const std::size_t step_cov;

    __host__ __device__ Float operator()(const std::size_t index, const Float original) const {
        const auto peak_idx = index / step_cov;
        const auto ij = index % step_cov;
        const auto i = ij / dim;
        const auto j = ij % dim;
        if (i == x_idx or i == p_idx or j == x_idx or j == p_idx) return original;

        const auto offset = peak_idx * dim;
        return original -
               cov_copy_ptr[offset + i] * cov_copy_ptr[offset + j] / cov_copy_ptr[offset + x_idx];
    }
};

}  // namespace internal

// MARK: StateGPU
/**
 * @brief Gaussian parameters for multiple peaks in multiple modes on the GPU.
 *
 * @tparam Float
 * @details This class is only used for processing on the GPU.
 */
template <std::floating_point Float>
class StateGPU {
public:
    /**
     * @brief Construct a new StateGPU object.
     *
     * @param modes The vector of 'SingleModeMultiPeak'
     * @details This constructor is faster than the one that takes 'State' as an argument, as it
     * requires less upload data thanks to the sparse covariance matrix, which reflects the absence
     * of correlations between modes.
     */
    explicit StateGPU(const std::vector<SingleModeMultiPeak<Float>>& modes) {
        if (modes.size() > static_cast<std::size_t>(std::numeric_limits<long>::max())) {
            throw SimulationError(error::InvalidState, "The size of argument peaks is too large");
        }

        n_modes_ = modes.size();
        auto sizes = std::vector<std::size_t>{};
        std::transform(modes.begin(), modes.end(), std::back_inserter(sizes),
                       [](const auto& x) { return x.peaks.size(); });
        n_peaks_ = std::accumulate(sizes.begin(), sizes.end(), 1u, std::multiplies<>());

        auto h_coeffs =
            thrust::host_vector<thrust::complex<Float>>(n_peaks_, thrust::complex<Float>());
        auto h_means = thrust::host_vector<thrust::complex<Float>>(n_peaks_ * (2 * n_modes_),
                                                                   thrust::complex<Float>());
        auto h_diags = thrust::host_vector<Float>();
        h_diags.reserve(n_peaks_ * (4 * n_modes_));
        auto h_index = thrust::host_vector<std::size_t>();
        h_index.reserve(n_peaks_ * (4 * n_modes_));

        std::size_t peak_idx = 0;
        auto current = std::vector<SingleModeSinglePeak<Float>>{};
        current.reserve(n_modes_);
        auto direct_product = [&modes, &h_coeffs, &h_means, &h_diags, &h_index, &peak_idx, &current,
                               this](auto self, const std::size_t depth = 0) {
            if (depth == modes.size()) {
                h_coeffs[IndexCoeff(peak_idx)] = std::accumulate(
                    std::ranges::views::transform(current, &SingleModeSinglePeak<Float>::coeff)
                        .begin(),
                    std::ranges::views::transform(current, &SingleModeSinglePeak<Float>::coeff)
                        .end(),
                    std::complex<Float>(1, 0), std::multiplies<>());
                for (long mode = 0; const auto& p : current) {
                    const auto p_idx = mode + static_cast<long>(n_modes_);

                    h_means[IndexMean(peak_idx, mode)] = p.mean(0);
                    h_means[IndexMean(peak_idx, p_idx)] = p.mean(1);
                    h_diags.push_back(p.cov(0, 0));
                    h_diags.push_back(p.cov(0, 1));
                    h_diags.push_back(p.cov(1, 0));
                    h_diags.push_back(p.cov(1, 1));
                    h_index.push_back(IndexCov(peak_idx, mode, mode));
                    h_index.push_back(IndexCov(peak_idx, mode, p_idx));
                    h_index.push_back(IndexCov(peak_idx, p_idx, mode));
                    h_index.push_back(IndexCov(peak_idx, p_idx, p_idx));
                    ++mode;
                }
                ++peak_idx;
                return;
            }
            for (const auto& peak : modes[depth].peaks) {
                current.push_back(peak);
                self(self, depth + 1);
                current.pop_back();
            }
        };
        direct_product(direct_product);

        const auto dim = 2 * NumModes();
        coeffs_ = h_coeffs;
        means_ = h_means;
        covs_ = thrust::device_vector<Float>(NumPeaks() * dim * dim, Float());
        thrust::device_vector<Float> d_diags = h_diags;
        thrust::device_vector<std::size_t> d_index = h_index;
        thrust::scatter(d_diags.begin(), d_diags.end(), d_index.begin(), covs_.begin());
        tmp_buffers_ = internal::GPUTempBuffers<Float>(n_modes_, n_peaks_);
    }

    /**
     * @brief Construct a new StateGPU object.
     *
     * @param from Bosonic state.
     * @details This class is instantiated from 'State<Float>'.
     */
    explicit StateGPU(const bosim::State<Float>& from) {
        n_modes_ = from.NumModes();
        n_peaks_ = from.NumPeaks();

        auto coeffs = thrust::host_vector<thrust::complex<Float>>();
        auto means = thrust::host_vector<thrust::complex<Float>>();
        auto covs = thrust::host_vector<Float>();
        for (std::size_t i = 0; i < n_peaks_; ++i) {
            coeffs.push_back(from.GetCoeff(i));

            means.insert(means.end(), from.GetMean(i).begin(), from.GetMean(i).end());

            auto cov = internal::MatrixToVector(from.GetCov(i));
            covs.insert(covs.end(), cov.begin(), cov.end());
        }

        coeffs_ = coeffs;
        means_ = means;
        covs_ = covs;
        tmp_buffers_ = internal::GPUTempBuffers<Float>(n_modes_, n_peaks_);
    }

    /**
     * @brief Convert to 'State<Float>'
     *
     * @return State<Float>
     */
    State<Float> ToState() {
        thrust::host_vector<thrust::complex<Float>> coeffs = coeffs_;
        thrust::host_vector<thrust::complex<Float>> means = means_;
        thrust::host_vector<Float> covs = covs_;

        const std::size_t mean_step = 2 * n_modes_;
        const std::size_t cov_step = 4 * n_modes_ * n_modes_;
        const std::size_t cov_rows = 2 * n_modes_;

        std::vector<MultiModeSinglePeak<Float>> peaks;
        for (std::size_t i = 0; i < NumPeaks(); ++i) {
            // mean
            auto mean = VectorC<Float>(mean_step);
            std::copy(means.begin() + mean_step * i, means.begin() + mean_step * (i + 1),
                      mean.begin());

            // cov
            auto cov_vector = std::vector<Float>(cov_step);
            std::copy(covs.begin() + cov_step * i, covs.begin() + cov_step * (i + 1),
                      cov_vector.begin());
            const auto cov = internal::VectorToMatrixD(cov_vector, cov_rows, cov_rows);

            peaks.emplace_back(MultiModeSinglePeak<Float>(coeffs[i], mean, cov));
        }
        auto ret = State(peaks);
        ret.GetEngine() = GetEngine();
        ret.SetShot(GetShot());
        return ret;
    }

    std::size_t Bytes() const {
        if (coeffs_.empty()) return 0;
        return internal::GetBytes(coeffs_) + internal::GetBytes(means_) +
               internal::GetBytes(covs_) + tmp_buffers_.Bytes();
    }

    std::size_t NumModes() const { return n_modes_; }
    /**
     * @brief Get the number of peaks.
     *
     * @return std::size_t
     */
    std::size_t NumPeaks() const { return n_peaks_; }

    void SetSeed(std::uint64_t seed) { engine_.seed(seed); }
    void SetShot(const std::size_t shot) { shot_ = shot; }
    std::mt19937_64& GetEngine() const { return engine_; }
    std::size_t GetShot() const { return shot_; }

    void DisplacementGPU(std::size_t mode, Float x, Float p, cudaStream_t stream);
    template <EigenMatrix Matrix>
    void UpdateStateGPU(const std::vector<std::size_t>& operated_indices, const Matrix& matrix,
                        cudaStream_t stream) {
        const auto operated_indices_size = operated_indices.size();

        auto& h_operated_indices = tmp_buffers_.HostOperatedIndices(operated_indices_size);
        auto& d_operated_indices = tmp_buffers_.OperatedIndices(operated_indices_size);
        auto& h_matrix = tmp_buffers_.HostMatrix(operated_indices_size);
        auto& d_matrix = tmp_buffers_.Matrix(operated_indices_size);

        // Set host vectors, which are pinned.
        std::copy(operated_indices.begin(), operated_indices.end(), h_operated_indices.begin());
        for (auto r = 0; r < matrix.rows(); r++) {
            for (auto c = 0; c < matrix.cols(); ++c) {
                h_matrix[r * matrix.cols() + c] = matrix(r, c);
            }
        }

        // Copy from host to device.
        thrust::copy(thrust::cuda::par.on(stream), h_operated_indices.begin(),
                     h_operated_indices.end(), d_operated_indices.begin());
        thrust::copy(thrust::cuda::par.on(stream), h_matrix.begin(), h_matrix.end(),
                     d_matrix.begin());
        UpdateStateGPUImpl(operated_indices_size, stream);
    };
    Float HomodyneSingleModeXGPU(std::size_t mode, cudaStream_t stream);
    Float HomodyneSingleModeXGPU(std::size_t mode, Float val, cudaStream_t stream);  // for debug
    Float HomodyneSingleModeGPU(std::size_t mode, Float theta, cudaStream_t stream);
    Float HomodyneSingleModeGPU(std::size_t mode, Float theta, Float val,
                                cudaStream_t stream);  // for debug
    void ReplaceBySqueezedStateGPU(std::size_t mode, Float squeezing_level, Float phi,
                                   cudaStream_t stream);

private:
    std::size_t n_modes_;
    std::size_t n_peaks_;
    thrust::device_vector<thrust::complex<Float>> coeffs_;
    thrust::device_vector<thrust::complex<Float>> means_;
    thrust::device_vector<Float> covs_;
    mutable internal::GPUTempBuffers<Float> tmp_buffers_;
    mutable std::mt19937_64 engine_;
    std::size_t shot_ = 0;

public:
    std::size_t StepMean() const { return 2 * n_modes_; }
    std::size_t StepCov() const { return 4 * n_modes_ * n_modes_; }
    std::size_t IndexCoeff(const std::size_t peak_idx) const { return peak_idx; }
    std::size_t IndexMean(const std::size_t peak_idx, const std::size_t i) const {
        return peak_idx * StepMean() + i;
    }
    std::size_t IndexCov(const std::size_t peak_idx, const std::size_t i,
                         const std::size_t j) const {
        return peak_idx * StepCov() + i * (2 * n_modes_) + j;
    }

    void UpdateStateGPUImpl(std::size_t operated_indices_size, cudaStream_t stream);
    internal::SamplingResult<Float> Sampling(std::size_t, cudaStream_t) const;
    void PostSelectBySamplingResult(std::size_t, const internal::SamplingResult<Float>&,
                                    cudaStream_t);
    Float CalcProposalValue(const thrust::device_vector<std::uint8_t>& peak_types,
                            const thrust::device_vector<Float>& proposal_peak_coeffs,
                            std::size_t mode, Float x) const;
    Float CalcOriginalPdf(std::size_t mode, Float x,
                          thrust::device_vector<thrust::complex<Float>>& o_original_peak_vals,
                          cudaStream_t stream) const;

    void PostSelectByMeasuredValue(std::size_t, Float, cudaStream_t);  // for debug
};

template <std::floating_point Float>
void StateGPU<Float>::DisplacementGPU(std::size_t mode, Float x, Float p, cudaStream_t stream) {
    using namespace thrust::placeholders;

    auto& indices = tmp_buffers_.DisplacementIndices();
    auto& values = tmp_buffers_.DisplacementValues();

    // Compute the indices
    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
        thrust::make_counting_iterator<std::size_t>(NumPeaks()), indices.begin() + 0 * NumPeaks(),
        _1 * StepMean() + mode);  // x_idx
    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
        thrust::make_counting_iterator<std::size_t>(NumPeaks()), indices.begin() + 1 * NumPeaks(),
        _1 * StepMean() + mode + NumModes());  // p_idx

    // Update
    thrust::gather(thrust::cuda::par_nosync.on(stream), indices.begin(), indices.end(),
                   means_.begin(), values.begin());
    thrust::transform(
        thrust::cuda::par_nosync.on(stream), values.begin(), values.begin() + NumPeaks(),
        thrust::make_constant_iterator<thrust::complex<Float>>(x), values.begin() + 0 * NumPeaks(),
        thrust::plus<thrust::complex<Float>>());  // += x
    thrust::transform(thrust::cuda::par_nosync.on(stream), values.begin() + NumPeaks(),
                      values.end(), thrust::make_constant_iterator<thrust::complex<Float>>(p),
                      values.begin() + 1 * NumPeaks(),
                      thrust::plus<thrust::complex<Float>>());  // += p
    thrust::scatter(thrust::cuda::par_nosync.on(stream), values.begin(), values.end(),
                    indices.begin(), means_.begin());
}

template <std::floating_point Float>
void StateGPU<Float>::UpdateStateGPUImpl(const std::size_t operated_indices_size,
                                         cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::BinaryOperation>{GetShot()};
#endif

    using namespace internal;
    using namespace thrust::placeholders;

    const auto dim = 2 * NumModes();

    auto& operated_indices = tmp_buffers_.OperatedIndices(operated_indices_size);
    auto& matrix = tmp_buffers_.Matrix(operated_indices_size);

    thrust::complex<Float>* means_ptr = thrust::raw_pointer_cast(means_.data());
    Float* covs_ptr = thrust::raw_pointer_cast(covs_.data());
    const std::size_t* operated_indices_ptr = thrust::raw_pointer_cast(operated_indices.data());
    const Float* matrix_ptr = thrust::raw_pointer_cast(matrix.data());

    {  // Update mu
        auto& mu_operated_indices = tmp_buffers_.MuOperatedIndices(operated_indices_size);
        auto& mu_operated_values = tmp_buffers_.MuOperatedValues(operated_indices_size);

        // Compute 'mu_operated_indices'
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(mu_operated_indices.size()),
                          mu_operated_indices.begin(),
                          CalculateMuOperatedMappingFunctor{operated_indices_ptr,
                                                            operated_indices_size, StepMean()});

        // Compute 'mu_operated_values' and Update 'means_'
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(mu_operated_values.size()),
                          mu_operated_values.begin(),
                          ComputeMuOperatedFunctor{means_ptr, matrix_ptr, operated_indices_ptr,
                                                   operated_indices_size, StepMean()});
        thrust::scatter(thrust::cuda::par_nosync.on(stream), mu_operated_values.begin(),
                        mu_operated_values.end(), mu_operated_indices.begin(),
                        means_.begin());  // Store
    }
    {  // Update sigma
        auto& sigma_off_diagonal_indices =
            tmp_buffers_.SigmaOffDiagonalIndices(operated_indices_size);
        auto& sigma_off_diagonal_values =
            tmp_buffers_.SigmaOffDiagonalValues(operated_indices_size);
        auto& sigma_operated_indices = tmp_buffers_.SigmaOperatedIndices(operated_indices_size);
        auto& sigma_operated_values = tmp_buffers_.SigmaOperatedValues(operated_indices_size);

        // Compute 'sigma_off_diagonal_values'
        thrust::transform(
            thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
            thrust::make_counting_iterator<std::size_t>(sigma_off_diagonal_values.size()),
            sigma_off_diagonal_values.begin(),
            ComputeSigmaOffDiagonalFunctor{covs_ptr, matrix_ptr, operated_indices_ptr,
                                           operated_indices_size, dim, StepCov()});

        // Compute 'sigma_operated_values'
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(sigma_operated_values.size()),
                          sigma_operated_values.begin(),
                          ComputeSigmaOperatedFunctor{covs_ptr, matrix_ptr, operated_indices_ptr,
                                                      operated_indices_size, dim, StepCov()});

        // NOTE: Because the areas partially overlap, storing must be in this order
        // Compute 'sigma_off_diagonal_indices' and Update 'covs_'
        thrust::transform(
            thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
            thrust::make_counting_iterator<std::size_t>(sigma_off_diagonal_indices.size()),
            sigma_off_diagonal_indices.begin(),
            CalculateSigmaOffDiagonalMappingFunctor<true>{operated_indices_ptr,
                                                          operated_indices_size, dim, StepCov()});
        thrust::scatter(thrust::cuda::par_nosync.on(stream), sigma_off_diagonal_values.begin(),
                        sigma_off_diagonal_values.end(), sigma_off_diagonal_indices.begin(),
                        covs_.begin());  // store in sigma(i, j); ij-order
        thrust::transform(
            thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
            thrust::make_counting_iterator<std::size_t>(sigma_off_diagonal_indices.size()),
            sigma_off_diagonal_indices.begin(),
            CalculateSigmaOffDiagonalMappingFunctor<false>{operated_indices_ptr,
                                                           operated_indices_size, dim, StepCov()});
        thrust::scatter(thrust::cuda::par_nosync.on(stream), sigma_off_diagonal_values.begin(),
                        sigma_off_diagonal_values.end(), sigma_off_diagonal_indices.begin(),
                        covs_.begin());  // store in sigma(j, i); ji-order

        // Compute 'sigma_operated_indices' and Update 'covs_'
        thrust::transform(
            thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
            thrust::make_counting_iterator<std::size_t>(sigma_operated_indices.size()),
            sigma_operated_indices.begin(),
            CalculateSigmaOperatedMappingFunctor{operated_indices_ptr, operated_indices_size, dim,
                                                 StepCov()});
        thrust::scatter(thrust::cuda::par_nosync.on(stream), sigma_operated_values.begin(),
                        sigma_operated_values.end(), sigma_operated_indices.begin(), covs_.begin());
    }
}

template <std::floating_point Float>
Float StateGPU<Float>::HomodyneSingleModeXGPU(const std::size_t mode, cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::HomodyneSingleModeX>{GetShot()};
#endif

    const auto result = Sampling(mode, stream);
    PostSelectBySamplingResult(mode, result, stream);
    return result.measured_x;
}
template <std::floating_point Float>
Float StateGPU<Float>::HomodyneSingleModeXGPU(const std::size_t mode, Float val,
                                              cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::HomodyneSingleModeX>{GetShot()};
#endif

    PostSelectByMeasuredValue(mode, val, stream);
    return val;
}
template <std::floating_point Float>
Float StateGPU<Float>::HomodyneSingleModeGPU(const std::size_t mode, const Float theta,
                                             cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::HomodyneSingleMode>{GetShot()};
#endif

    {  // rot_front
        const auto rot_front =
            operation::PhaseRotation<Float>(mode, -std::numbers::pi_v<Float> / 2 + theta);
        auto operated_indices = rot_front.Modes();
        for (auto mode : rot_front.Modes()) { operated_indices.push_back(mode + NumModes()); }
        UpdateStateGPU(operated_indices, rot_front.GetOperationMatrix(), stream);
    }
    return HomodyneSingleModeXGPU(mode, stream);
}
template <std::floating_point Float>
Float StateGPU<Float>::HomodyneSingleModeGPU(const std::size_t mode, const Float theta, Float val,
                                             cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::HomodyneSingleMode>{GetShot()};
#endif

    {  // rot_front
        const auto rot_front =
            operation::PhaseRotation<Float>(mode, -std::numbers::pi_v<Float> / 2 + theta);
        auto operated_indices = rot_front.Modes();
        for (auto mode : rot_front.Modes()) { operated_indices.push_back(mode + NumModes()); }
        UpdateStateGPU(operated_indices, rot_front.GetOperationMatrix(), stream);
    }
    return HomodyneSingleModeXGPU(mode, val, stream);
}

/**
 * @brief Sampling
 *
 * @tparam Float
 * @param mode to measure.
 * @param stream for executing kernel functions.
 * @return internal::SamplingResult<Float>
 * @details Compute values on the GPU and download the required values for Sampling. The number of
 * required values = O(NumPeaks). Sampling is performed on CPU.
 */
template <std::floating_point Float>
internal::SamplingResult<Float> StateGPU<Float>::Sampling(const std::size_t mode,
                                                          cudaStream_t stream) const {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::Sampling>{GetShot()};
#endif

    constexpr std::int32_t MaxAttemptsOfRejectionSampling = 10'000;

    using namespace internal;
    using namespace thrust::placeholders;

    auto& peak_types = tmp_buffers_.PeakTypes();
    thrust::transform(thrust::cuda::par_nosync.on(stream),
                      thrust::make_counting_iterator<std::size_t>(0),
                      thrust::make_counting_iterator<std::size_t>(NumPeaks()), peak_types.begin(),
                      internal::CalcPeakTypesFunctor{thrust::raw_pointer_cast(coeffs_.data()),
                                                     thrust::raw_pointer_cast(means_.data()), mode,
                                                     mode + NumModes(), StepMean()});

    auto& proposal_peak_coeffs = tmp_buffers_.ProposalPeakCoeffs();
    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
        thrust::make_counting_iterator<std::size_t>(NumPeaks()), peak_types.begin(),
        proposal_peak_coeffs.begin(),
        internal::CalcProposalPeakCoeffsFunctor{
            thrust::raw_pointer_cast(coeffs_.data()), thrust::raw_pointer_cast(means_.data()),
            thrust::raw_pointer_cast(covs_.data()), mode, mode * (2 * NumModes()) + mode,
            StepMean(), StepCov()});

    // Download required values
    const thrust::host_vector<Float> h_proposal_peak_coeffs = proposal_peak_coeffs;

    const thrust::host_vector<thrust::complex<Float>> mus = [this, mode, stream]() {
        auto& indices = tmp_buffers_.SamplingMuIndices();
        auto& values = tmp_buffers_.SamplingMuValues();

        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(indices.size()),
                          indices.begin(), StepMean() * _1 + mode);
        thrust::gather(thrust::cuda::par_nosync.on(stream), indices.begin(), indices.end(),
                       means_.begin(), values.begin());
        return values;
    }();

    const thrust::host_vector<Float> sigmas = [this, mode, stream]() {
        auto& indices = tmp_buffers_.SamplingSigmaIndices();
        auto& values = tmp_buffers_.SamplingSigmaValues();

        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(indices.size()),
                          indices.begin(), StepCov() * _1 + mode * (2 * NumModes()) + mode);
        thrust::gather(thrust::cuda::par_nosync.on(stream), indices.begin(), indices.end(),
                       covs_.begin(), values.begin());
        return values;
    }();

    // Sampling is performed on the CPU
    auto peak_selector = std::discrete_distribution<std::size_t>{h_proposal_peak_coeffs.begin(),
                                                                 h_proposal_peak_coeffs.end()};
    auto ret = SamplingResult<Float>{};
    ret.original_peak_vals.resize(NumPeaks());
    for (std::int32_t i_attempt = 1; i_attempt <= MaxAttemptsOfRejectionSampling; ++i_attempt) {
        // Peak sampling
        const std::size_t sampled_peak_idx = peak_selector(GetEngine());

        // Gaussian sampling
        const auto mu = mus[sampled_peak_idx];
        const auto sigma = sigmas[sampled_peak_idx];
        auto sampled_pdf = std::normal_distribution<Float>{mu.real(), std::sqrt(sigma)};
        const Float sampled_x = sampled_pdf(GetEngine());

        // Uniform sampling
        const Float proposal_val =
            CalcProposalValue(peak_types, proposal_peak_coeffs, mode, sampled_x);
        auto uniform_pdf = std::uniform_real_distribution<Float>{0, proposal_val};
        const Float y = uniform_pdf(GetEngine());

        // Accept 'sampled_x' if 'y' is smaller than or equal to the original PDF value.
        const Float original_pdf_val =
            CalcOriginalPdf(mode, sampled_x, ret.original_peak_vals, stream);
        if (y <= original_pdf_val) {
            ret.measured_x = sampled_x;
            ret.original_pdf_val = original_pdf_val;
            return ret;
        }
    }

    throw SimulationError(
        error::Unknown,
        fmt::format("Rejection sampling failed after {} attempts. No valid samples were accepted.",
                    MaxAttemptsOfRejectionSampling));
}

template <std::floating_point Float>
void StateGPU<Float>::PostSelectBySamplingResult(const std::size_t mode,
                                                 const internal::SamplingResult<Float>& result,
                                                 cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::PostSelectBySamplingResult>{GetShot()};
#endif

    using namespace thrust::placeholders;

    const auto dim = 2 * NumModes();
    const auto x_idx = mode;
    const auto p_idx = mode + NumModes();

    auto& cov_copy = tmp_buffers_.CovCopyValues();
    {  // cov_copy = cov.col(x_idx)
        auto& indices = tmp_buffers_.CovCopyIndices();
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(indices.size()),
                          indices.begin(), _1 / dim * StepCov() + _1 % dim * dim + x_idx);
        thrust::gather(thrust::cuda::par_nosync.on(stream), indices.begin(), indices.end(),
                       covs_.begin(), cov_copy.begin());
    }

    {  // Update meanA
        const Float* covs_ptr = thrust::raw_pointer_cast(covs_.data());
        const Float* cov_copy_ptr = thrust::raw_pointer_cast(cov_copy.data());
        thrust::complex<Float>* means_ptr = thrust::raw_pointer_cast(means_.data());

        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(means_.size()),
                          means_.begin(), means_.begin(),
                          internal::UpdateMeanAFunctor{
                              covs_ptr,
                              cov_copy_ptr,
                              means_ptr,
                              thrust::complex<Float>(result.measured_x),
                              dim,
                              x_idx,
                              p_idx,
                              StepMean(),
                              StepCov(),
                          });
    }
    {  // Update covA
        const Float* cov_copy_ptr = thrust::raw_pointer_cast(cov_copy.data());
        Float* covs_ptr = thrust::raw_pointer_cast(covs_.data());

        thrust::transform(
            thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
            thrust::make_counting_iterator<std::size_t>(covs_.size()), covs_.begin(), covs_.begin(),
            internal::UpdateCovAFunctor{cov_copy_ptr, dim, x_idx, p_idx, StepCov()});
    }

    // Update peak_coeffs
    thrust::transform(thrust::cuda::par_nosync.on(stream), result.original_peak_vals.begin(),
                      result.original_peak_vals.end(), coeffs_.begin(),
                      _1 / result.original_pdf_val);
    {  // Update mean_B
        auto& indices = tmp_buffers_.MeanBIndices();
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks()),
                          indices.begin() + 0 * NumPeaks(),
                          _1 * StepMean() + x_idx);  // index of mean(x_idx)
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks()),
                          indices.begin() + 1 * NumPeaks(),
                          _1 * StepMean() + p_idx);  // index of mean(p_idx)

        thrust::scatter(thrust::cuda::par_nosync.on(stream), thrust::constant_iterator<Float>(0),
                        thrust::constant_iterator<Float>(0) + indices.size(), indices.begin(),
                        means_.begin());  // means(indices(*)) = 0
    }
    {  // Update cov_AB
        auto& indices = tmp_buffers_.CovABIndices();
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks() * dim),
                          indices.begin() + 0 * NumPeaks() * dim,
                          _1 / dim * StepCov() + x_idx * dim + _1 % dim);  // index of cov(x_idx, i)
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks() * dim),
                          indices.begin() + 1 * NumPeaks() * dim,
                          _1 / dim * StepCov() + p_idx * dim + _1 % dim);  // index of cov(p_idx, i)
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks() * dim),
                          indices.begin() + 2 * NumPeaks() * dim,
                          _1 / dim * StepCov() + _1 % dim * dim + x_idx);  // index of cov(i, x_idx)
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks() * dim),
                          indices.begin() + 3 * NumPeaks() * dim,
                          _1 / dim * StepCov() + _1 % dim * dim + p_idx);  // index of cov(i, p_idx)
        thrust::scatter(thrust::cuda::par_nosync.on(stream), thrust::constant_iterator<Float>(0),
                        thrust::constant_iterator<Float>(0) + indices.size(), indices.begin(),
                        covs_.begin());  // covs(indices(*)) = 0
    }
    {  // Update cov_B
        auto& indices = tmp_buffers_.CovBIndices();
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks()),
                          indices.begin() + 0 * NumPeaks(),
                          _1 * StepCov() + x_idx * dim + x_idx);  // index of cov(x_idx, x_idx)
        thrust::transform(thrust::cuda::par_nosync.on(stream),
                          thrust::make_counting_iterator<std::size_t>(0),
                          thrust::make_counting_iterator<std::size_t>(NumPeaks()),
                          indices.begin() + 1 * NumPeaks(),
                          _1 * StepCov() + p_idx * dim + p_idx);  // index of cov(p_idx, p_idx)
        thrust::scatter(
            thrust::cuda::par_nosync.on(stream), thrust::constant_iterator<Float>(Hbar<Float> / 2),
            thrust::constant_iterator<Float>(Hbar<Float> / 2) + indices.size(), indices.begin(),
            covs_.begin());  // covs(indices(*)) = Hbar<Float> / 2
    }
}

template <std::floating_point Float>
Float StateGPU<Float>::CalcProposalValue(const thrust::device_vector<std::uint8_t>& peak_types,
                                         const thrust::device_vector<Float>& proposal_peak_coeffs,
                                         const std::size_t mode, Float x) const {
    auto iter = thrust::make_zip_iterator(thrust::make_counting_iterator<std::size_t>(0),
                                          peak_types.begin(), proposal_peak_coeffs.begin());
    return thrust::transform_reduce(
        iter, iter + NumPeaks(),
        internal::CalcProposalValueFunctor{thrust::raw_pointer_cast(means_.data()),
                                           thrust::raw_pointer_cast(covs_.data()), x, mode,
                                           mode * (2 * NumModes()) + mode, StepMean(), StepCov()},
        Float(0), thrust::plus<Float>());
}

template <std::floating_point Float>
Float StateGPU<Float>::CalcOriginalPdf(
    const std::size_t mode, const Float x,
    thrust::device_vector<thrust::complex<Float>>& o_original_peak_vals,
    cudaStream_t stream) const {
    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator<std::size_t>(0),
        thrust::make_counting_iterator<std::size_t>(NumPeaks()), o_original_peak_vals.begin(),
        internal::CalcOriginalPdfFunctor{thrust::raw_pointer_cast(coeffs_.data()),
                                         thrust::raw_pointer_cast(means_.data()),
                                         thrust::raw_pointer_cast(covs_.data()), x, mode,
                                         mode * (2 * NumModes()) + mode, StepMean(), StepCov()});
    const auto original_pdf_val =
        thrust::reduce(thrust::cuda::par_nosync.on(stream), o_original_peak_vals.begin(),
                       o_original_peak_vals.end());
    return original_pdf_val.real();
}

template <std::floating_point Float>
void StateGPU<Float>::PostSelectByMeasuredValue(const std::size_t mode, const Float sampled_x,
                                                cudaStream_t stream) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::PostSelectByMeasuredValue>{GetShot()};
#endif

    auto result = internal::SamplingResult<Float>{};
    result.original_peak_vals.resize(NumPeaks());
    result.measured_x = sampled_x;
    result.original_pdf_val = CalcOriginalPdf(mode, sampled_x, result.original_peak_vals, stream);

    PostSelectBySamplingResult(mode, result, stream);
}

template <std::floating_point Float>
void StateGPU<Float>::ReplaceBySqueezedStateGPU(const std::size_t mode, const Float squeezing_level,
                                                const Float phi, cudaStream_t stream) {
    using namespace thrust::placeholders;

    auto& mode_xp_indices = tmp_buffers_.ModeXPIndices();
    mode_xp_indices = std::vector<std::size_t>{mode, mode + NumModes()};

    auto& squeezed_cov = tmp_buffers_.SqueezedCov();
    const auto h_squeezed_cov = GenerateSqueezedCov(squeezing_level, phi);
    squeezed_cov = internal::MatrixToVector(h_squeezed_cov);

    // Indices of x and p means of `mode` of all peaks
    auto& operated_mean_indices = tmp_buffers_.OperatedMeanIndices();

    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator(std::size_t{0}),
        thrust::make_counting_iterator(NumPeaks()), operated_mean_indices.begin() + 0 * NumPeaks(),
        _1 * StepMean() + mode);  // x_idx
    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator(std::size_t{0}),
        thrust::make_counting_iterator(NumPeaks()), operated_mean_indices.begin() + 1 * NumPeaks(),
        _1 * StepMean() + mode + NumModes());  // p_idx

    // Set the x and p means of `mode` of all peaks to 0
    const auto zero_iterator = thrust::make_constant_iterator(thrust::complex<Float>(0.0));
    thrust::scatter(thrust::cuda::par_nosync.on(stream), zero_iterator,
                    zero_iterator + operated_mean_indices.size(), operated_mean_indices.begin(),
                    means_.begin());

    // Indices of covariance matrix elements of `mode` of all peaks
    auto& operated_cov_indices = tmp_buffers_.OperatedCovIndices();

    thrust::transform(
        thrust::cuda::par_nosync.on(stream), thrust::make_counting_iterator(std::size_t{0}),
        thrust::make_counting_iterator(operated_cov_indices.size()), operated_cov_indices.begin(),
        internal::CalculateSigmaOperatedMappingFunctor{
            thrust::raw_pointer_cast(mode_xp_indices.data()), mode_xp_indices.size(),
            2 * NumModes(), StepCov()});

    // Squeezed covariance matrices for all peaks
    auto& squeezed_covs = tmp_buffers_.SqueezedCovs();

    thrust::transform(thrust::cuda::par_nosync.on(stream),
                      thrust::make_counting_iterator(std::size_t{0}),
                      thrust::make_counting_iterator(squeezed_covs.size()), squeezed_covs.begin(),
                      internal::CalculateSqueezedCovFunctor{
                          thrust::raw_pointer_cast(squeezed_cov.data()), squeezed_cov.size()});

    // Update the covariance matrices of `mode` of all peaks
    thrust::scatter(thrust::cuda::par_nosync.on(stream), squeezed_covs.begin(), squeezed_covs.end(),
                    operated_cov_indices.begin(), covs_.begin());
}

}  // namespace bosim::gpu
