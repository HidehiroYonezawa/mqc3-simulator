#pragma once

#include "bosim/base/timer.h"
#include "bosim/simulate/graph_repr.h"
#include "bosim/simulate/graph_result.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"

#include "mqc3_cloud/program/v1/graph.pb.h"

namespace bosim::graph {

using mqc3_cloud::program::v1::GraphRepresentation;
using mqc3_cloud::program::v1::GraphResult;

/**
 * @brief Simulate a graph representation.
 *
 * @tparam Float The floating point type.
 * @param settings Simulation settings.
 * @param graph_repr Graph representation.
 * @param squeezing_level Squeezing level of a quantum computer in dB.
 * @return Simulation result.
 */
template <std::floating_point Float>
auto GraphSimulate(const SimulateSettings& settings, const GraphRepresentation& graph_repr,
                   const Float squeezing_level) -> GraphResult {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile_initialization = profiler::Profiler<profiler::Section::InitializeGraphModes>{};
#endif
    const auto initial_state = [n_local_macronodes = graph_repr.n_local_macronodes(),
                                squeezing_level]() {
        auto modes = std::vector<SingleModeMultiPeak<Float>>(n_local_macronodes + 1);
        for (auto& mode : modes) {
            mode = SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level);
        }
        return State<Float>{modes};
    }();
#ifdef BOSIM_BUILD_BENCHMARK
    profile_initialization.End();
#endif
    auto circuit = GraphCircuit<Float>(graph_repr, squeezing_level);
    const auto result = Simulate<Float>(settings, circuit, initial_state);
    return ExtractGraphResult(result, graph_repr);
}
}  // namespace bosim::graph
