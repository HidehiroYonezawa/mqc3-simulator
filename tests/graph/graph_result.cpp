#include "bosim/simulate/graph_result.h"

#include <gtest/gtest.h>

#include "bosim/result.h"

#include "mqc3_cloud/program/v1/graph.pb.h"

#include "graph_repr.h"

TEST(GraphRepresentation, CountOperations) {
    using namespace mqc3_cloud::program::v1;

    const auto unary_operation_types = std::vector{
        GraphOperation::OPERATION_TYPE_PHASE_ROTATION,
        GraphOperation::OPERATION_TYPE_SHEAR_X_INVARIANT,
        GraphOperation::OPERATION_TYPE_SHEAR_P_INVARIANT,
        GraphOperation::OPERATION_TYPE_SQUEEZING,
        GraphOperation::OPERATION_TYPE_SQUEEZING_45,
    };
    for (const auto op_type : unary_operation_types) {
        auto graph_op = GraphOperation{};
        graph_op.set_type(op_type);
        EXPECT_EQ(4, bosim::graph::CountOperations(graph_op));

        graph_op.mutable_displacement_k_minus_1()->set_x(1.0f);
        EXPECT_EQ(4, bosim::graph::CountOperations(graph_op));
    }

    const auto binary_operation_types = std::vector{
        GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST,
        GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND,
        GraphOperation::OPERATION_TYPE_CONTROLLED_Z,
        GraphOperation::OPERATION_TYPE_BEAM_SPLITTER,
        GraphOperation::OPERATION_TYPE_TWO_MODE_SHEAR,
        GraphOperation::OPERATION_TYPE_MANUAL,
    };
    for (const auto op_type : binary_operation_types) {
        auto graph_op = GraphOperation{};
        graph_op.set_type(op_type);
        EXPECT_EQ(3, bosim::graph::CountOperations(graph_op));

        graph_op.mutable_displacement_k_minus_n()->set_x(1.0f);
        EXPECT_EQ(3, bosim::graph::CountOperations(graph_op));
    }

    const auto measurement_operation_types = std::vector{
        GraphOperation::OPERATION_TYPE_MEASUREMENT,
        GraphOperation::OPERATION_TYPE_INITIALIZATION,
    };
    for (const auto op_type : measurement_operation_types) {
        auto graph_op = GraphOperation{};
        graph_op.set_type(op_type);
        EXPECT_EQ(6, bosim::graph::CountOperations(graph_op));

        graph_op.mutable_displacement_k_minus_1()->set_x(1.0f);
        graph_op.mutable_displacement_k_minus_n()->set_x(1.0f);
        EXPECT_EQ(6, bosim::graph::CountOperations(graph_op));
    }

    {
        auto wiring_op = GraphOperation{};
        wiring_op.set_type(GraphOperation::OPERATION_TYPE_WIRING);
        EXPECT_EQ(2, bosim::graph::CountOperations(wiring_op));

        wiring_op.mutable_displacement_k_minus_1()->set_p(1.0f);
        wiring_op.mutable_displacement_k_minus_n()->set_p(1.0f);
        EXPECT_EQ(2, bosim::graph::CountOperations(wiring_op));
    }
}

auto ConstructTestGraph() {
    using namespace mqc3_cloud::program::v1;

    // Construct a graph representation.
    GraphRepresentation graph_repr;
    graph_repr.set_n_local_macronodes(5);
    graph_repr.set_n_steps(2);

    // Create displacement objects for testing.
    GraphOperation::Displacement displacement_x;
    displacement_x.set_x(-0.5f);
    displacement_x.set_p(0.0f);
    GraphOperation::Displacement displacement_p;
    displacement_p.set_x(0.0f);
    displacement_p.set_p(1.0f);
    GraphOperation::Displacement displacement_xp;
    displacement_xp.set_x(2.0f);
    displacement_xp.set_p(-0.25f);

    // Add operations to the graph representation.
    // op1
    AddOperationToGraphRepr(graph_repr, 0, false, std::vector{std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_INITIALIZATION, displacement_x,
                            std::nullopt, true);
    // op2
    AddOperationToGraphRepr(graph_repr, 1, true, std::vector{std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_PHASE_ROTATION, std::nullopt,
                            displacement_p);
    // op3
    AddOperationToGraphRepr(graph_repr, 3, true, std::vector{0.25f},
                            GraphOperation::OPERATION_TYPE_INITIALIZATION, displacement_xp,
                            displacement_xp, false);
    // op4
    AddOperationToGraphRepr(graph_repr, 4, false, std::vector{1.0f, 2.0f, 3.0f, 4.0f},
                            GraphOperation::OPERATION_TYPE_MANUAL, std::nullopt, std::nullopt);
    // op5
    AddOperationToGraphRepr(graph_repr, 5, false, std::vector{0.25f},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, std::nullopt, std::nullopt,
                            true);
    // op6
    AddOperationToGraphRepr(graph_repr, 7, true, std::vector{0.25f},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, std::nullopt, std::nullopt,
                            false);

    EXPECT_EQ(6, graph_repr.operations().size());

    return graph_repr;
}

template <std::floating_point Float>
auto ConstructTestCircuitResult() {
    auto shot_result = bosim::ShotResult<Float>{.state = std::nullopt,
                                                .measured_values = std::map<std::size_t, Float>{}};
    const auto measurement_op_indices =
        std::unordered_set<std::size_t>{1, 4, 13, 16, 22, 25, 30, 33};
    for (auto op_idx : measurement_op_indices) {
        shot_result.measured_values[op_idx] = static_cast<Float>(op_idx);
    }

    return bosim::Result<Float>{.result = std::vector{shot_result, shot_result},
                                .elapsed_ms = bosim::Ms{1000}};
}

TEST(GraphRepresentation, ConstructMeasurementOpMap) {
    const auto graph_repr = ConstructTestGraph();
    const auto measurement_op_idx_to_macronode_info =
        bosim::graph::ConstructMeasurementOpMap(graph_repr);
    EXPECT_EQ(4, measurement_op_idx_to_macronode_info.size());

    const auto m1 = measurement_op_idx_to_macronode_info.at(1);
    EXPECT_EQ(0, m1.macronode);
    EXPECT_TRUE(m1.is_up_mode_measured);

    const auto m2 = measurement_op_idx_to_macronode_info.at(4);
    EXPECT_EQ(0, m2.macronode);
    EXPECT_FALSE(m2.is_up_mode_measured);

    const auto m5 = measurement_op_idx_to_macronode_info.at(22);
    EXPECT_EQ(5, m5.macronode);
    EXPECT_TRUE(m5.is_up_mode_measured);

    const auto m6 = measurement_op_idx_to_macronode_info.at(25);
    EXPECT_EQ(5, m6.macronode);
    EXPECT_FALSE(m6.is_up_mode_measured);
}

TEST(GraphRepresentation, ExtractGraphResult) {
    const auto graph_repr = ConstructTestGraph();
    const auto circuit_result = ConstructTestCircuitResult<double>();
    const auto graph_result = bosim::graph::ExtractGraphResult(circuit_result, graph_repr);
    EXPECT_EQ(2, graph_result.measured_vals_size());
    for (const auto& shot_measured_vals : graph_result.measured_vals()) {
        EXPECT_EQ(2, shot_measured_vals.measured_vals_size());
        const auto sorted_measured_vals = [&shot_measured_vals]() {
            auto sorted_measured_vals = std::vector(shot_measured_vals.measured_vals().begin(),
                                                    shot_measured_vals.measured_vals().end());
            std::ranges::sort(sorted_measured_vals,
                              [](const auto& a, const auto& b) { return a.index() < b.index(); });
            return sorted_measured_vals;
        }();
        EXPECT_EQ(0, sorted_measured_vals.at(0).index());
        EXPECT_EQ(1.0f, sorted_measured_vals.at(0).m_b());
        EXPECT_EQ(4.0f, sorted_measured_vals.at(0).m_d());

        EXPECT_EQ(5, sorted_measured_vals.at(1).index());
        EXPECT_EQ(22.0f, sorted_measured_vals.at(1).m_b());
        EXPECT_EQ(25.0f, sorted_measured_vals.at(1).m_d());
    }
}
