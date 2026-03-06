#pragma once

#include <concepts>
#include <optional>
#include <vector>

#include "bosim/base/timer.h"
#include "bosim/state.h"

namespace bosim {
template <std::floating_point Float>
struct ShotResult {
    std::optional<State<Float>> state;

    // Operation index vs measured value
    std::map<std::size_t, Float> measured_values;
};

template <std::floating_point Float>
struct Result {
    std::vector<ShotResult<Float>> result;
    Ms elapsed_ms;
};
}  // namespace bosim
