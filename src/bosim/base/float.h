#pragma once

#include <complex>
#include <concepts>

namespace bosim {
template <std::floating_point Float>
static constexpr Float Eps = static_cast<Float>(1e-5);

template <std::floating_point Float>
static constexpr bool IsReal(std::complex<Float> value) {
    return std::abs(value.imag()) < Eps<Float>;
}
}  // namespace bosim
