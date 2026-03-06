#pragma once

#include "bosim/circuit.h"
#include "bosim/result.h"
#include "bosim/simulate/settings.h"

namespace bosim::gpu {
static constexpr std::uint64_t MaxProductNPeaks = 100000000;

Result<double> SimulateSingleGPU(const SimulateSettings&, const Circuit<double>&,
                                 const State<double>&);
Result<float> SimulateSingleGPU(const SimulateSettings&, const Circuit<float>&,
                                const State<float>&);
Result<double> SimulateSingleGPU(const SimulateSettings&, const Circuit<double>&,
                                 const std::vector<SingleModeMultiPeak<double>>&);
Result<float> SimulateSingleGPU(const SimulateSettings&, const Circuit<float>&,
                                const std::vector<SingleModeMultiPeak<float>>&);
Result<double> SimulateMultiGPU(const SimulateSettings&, const Circuit<double>&,
                                const State<double>&);
Result<float> SimulateMultiGPU(const SimulateSettings&, const Circuit<float>&, const State<float>&);
Result<double> SimulateMultiGPU(const SimulateSettings&, const Circuit<double>&,
                                const std::vector<SingleModeMultiPeak<double>>&);
Result<float> SimulateMultiGPU(const SimulateSettings&, const Circuit<float>&,
                               const std::vector<SingleModeMultiPeak<float>>&);
}  // namespace bosim::gpu
