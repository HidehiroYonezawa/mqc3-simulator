#include "bosim/simulate/simulate.h"

namespace bosim {
template Result<double> Simulate(const SimulateSettings&, Circuit<double>&, const State<double>&);
template Result<float> Simulate(const SimulateSettings&, Circuit<float>&, const State<float>&);
template Result<double> Simulate(const SimulateSettings&, Circuit<double>&,
                                 const std::vector<SingleModeMultiPeak<double>>&);
template Result<float> Simulate(const SimulateSettings&, Circuit<float>&,
                                const std::vector<SingleModeMultiPeak<float>>&);
}  // namespace bosim
