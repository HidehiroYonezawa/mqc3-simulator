#include "bosim/simulate/graph_simulate.h"

#include <gtest/gtest.h>

#include "bosim/python/manager.h"
#include "bosim/simulate/settings.h"

#include "mqc3_cloud/program/v1/graph.pb.h"

#include "graph_repr.h"

TEST(GraphRepresentation, GraphSimulate) {
    using namespace mqc3_cloud::program::v1;

    const auto env = bosim::python::PythonEnvironment();

    // Construct a graph representation.
    auto graph_repr = GraphRepresentation{};
    graph_repr.set_n_local_macronodes(5);
    graph_repr.set_n_steps(5);

    // op1
    AddOperationToGraphRepr(graph_repr, 0, false, std::vector{0.5f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_INITIALIZATION, std::nullopt,
                            std::nullopt, false);
    // op2
    AddOperationToGraphRepr(graph_repr, 5, false, std::vector{0.25f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_PHASE_ROTATION, std::nullopt,
                            std::nullopt);
    // op3
    AddOperationToGraphRepr(graph_repr, 10, false, std::vector{std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, std::nullopt, std::nullopt,
                            true);

    constexpr std::size_t NShots = std::uint32_t{100};
    constexpr auto SqueezingLevel = 10.0f;
    const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::CPUSingleThread,
                                                  bosim::StateSaveMethod::None};
    const auto result = bosim::graph::GraphSimulate<float>(settings, graph_repr, SqueezingLevel);
    EXPECT_EQ(NShots, result.measured_vals_size());
    for (const auto& shot_measured_vals : result.measured_vals()) {
        EXPECT_EQ(1, shot_measured_vals.measured_vals_size());
        EXPECT_EQ(10, shot_measured_vals.measured_vals(0).index());
    }
}
