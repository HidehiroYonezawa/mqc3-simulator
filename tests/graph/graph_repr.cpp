#include "bosim/simulate/graph_repr.h"

#include <gtest/gtest.h>

#include "mqc3_cloud/program/v1/graph.pb.h"

#include "circuit/circuit_repr.h"
#include "graph_repr.h"

TEST(GraphRepresentation, GraphCircuit) {
    using namespace mqc3_cloud::program::v1;

    // Construct a graph representation.
    GraphRepresentation graph_repr;
    graph_repr.set_n_local_macronodes(3);
    graph_repr.set_n_steps(8);

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
    AddOperationToGraphRepr(graph_repr, 0, true, std::vector{std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_PHASE_ROTATION, displacement_x,
                            std::nullopt);
    // op2
    AddOperationToGraphRepr(graph_repr, 2, false, std::vector{0.5f},
                            GraphOperation::OPERATION_TYPE_SHEAR_X_INVARIANT, std::nullopt,
                            displacement_p);
    // op3
    AddOperationToGraphRepr(graph_repr, 3, false, std::vector{0.25f},
                            GraphOperation::OPERATION_TYPE_SHEAR_P_INVARIANT, std::nullopt,
                            std::nullopt);
    // op4
    AddOperationToGraphRepr(graph_repr, 4, true, std::vector{0.5f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_SQUEEZING, std::nullopt, std::nullopt);
    // op5
    AddOperationToGraphRepr(graph_repr, 5, true, std::vector{0.25f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_SQUEEZING_45, std::nullopt,
                            std::nullopt);
    // op6
    AddOperationToGraphRepr(graph_repr, 9, true, std::vector{0.125f},
                            GraphOperation::OPERATION_TYPE_CONTROLLED_Z, displacement_xp,
                            displacement_xp);
    // op7
    AddOperationToGraphRepr(
        graph_repr, 10, false, std::vector{1.0f, 0.125f * std::numbers::pi_v<float>},
        GraphOperation::OPERATION_TYPE_BEAM_SPLITTER, displacement_x, displacement_p);

    // op8
    AddOperationToGraphRepr(graph_repr, 11, true, std::vector{2.0f, 3.0f},
                            GraphOperation::OPERATION_TYPE_TWO_MODE_SHEAR, std::nullopt,
                            std::nullopt);
    // op9
    AddOperationToGraphRepr(graph_repr, 12, false,
                            std::vector{-0.25f * std::numbers::pi_v<float>, 0.0f,
                                        -0.25f * std::numbers::pi_v<float>, 0.0f},
                            GraphOperation::OPERATION_TYPE_MANUAL, std::nullopt, std::nullopt);
    // op10
    AddOperationToGraphRepr(
        graph_repr, 13, true,
        std::vector{0.5f * std::numbers::pi_v<float>, 0.25f * std::numbers::pi_v<float>, 0.0f},
        GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST, displacement_p, displacement_xp);
    // op11
    AddOperationToGraphRepr(
        graph_repr, 17, false,
        std::vector{0.5f * std::numbers::pi_v<float>, 0.25f * std::numbers::pi_v<float>, 0.0f},
        GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND, displacement_xp, displacement_x);

    // op12
    AddOperationToGraphRepr(
        graph_repr, 18, false,
        std::vector{0.25f * std::numbers::pi_v<float>, -0.25f * std::numbers::pi_v<float>, 0.0f},
        GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST, std::nullopt, std::nullopt);
    // op13
    AddOperationToGraphRepr(
        graph_repr, 21, true,
        std::vector{0.25f * std::numbers::pi_v<float>, -0.25f * std::numbers::pi_v<float>, 0.0f},
        GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND, std::nullopt, std::nullopt);
    // op14
    AddOperationToGraphRepr(graph_repr, 7, true, std::vector<float>{},
                            GraphOperation::OPERATION_TYPE_WIRING, displacement_p, displacement_x);
    // op15
    AddOperationToGraphRepr(graph_repr, 14, true, std::vector<float>{},
                            GraphOperation::OPERATION_TYPE_WIRING, std::nullopt, std::nullopt);
    // op16
    AddOperationToGraphRepr(graph_repr, 16, true, std::vector<float>{},
                            GraphOperation::OPERATION_TYPE_WIRING, std::nullopt, std::nullopt);
    // op17
    AddOperationToGraphRepr(graph_repr, 6, false, std::vector{0.125f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_INITIALIZATION, std::nullopt,
                            std::nullopt, true);
    // op18
    AddOperationToGraphRepr(graph_repr, 8, false, std::vector{-0.125f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, displacement_xp,
                            displacement_p, true);
    // op19
    AddOperationToGraphRepr(graph_repr, 22, true, std::vector{0.25f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_INITIALIZATION, displacement_p,
                            displacement_x, true);
    // op20
    AddOperationToGraphRepr(graph_repr, 23, true, std::vector{0.5f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, std::nullopt, std::nullopt,
                            true);

    // Convert the graph representation to a circuit object.
    constexpr auto SqueezingLevel = 10.0f;
    const auto expected_measurement_squeezing_level = SqueezingLevel - 10 * std::log10(2.0f);
    auto circuit = bosim::graph::GraphCircuit<float>(graph_repr, SqueezingLevel);
    const auto& circuit_ops = circuit.GetMutOperations();
    EXPECT_EQ(82, circuit_ops.size());

    // Check Operation types, parameters, and target modes.
    auto circuit_op_index = std::size_t{0};
    // macronode 0: op1 (displacement_x, )
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{displacement_x.x(), 0.0f});
    CheckOperation<bosim::operation::PhaseRotation<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::PhaseRotation<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{std::numbers::pi_v<float>});
    // macronode 1
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    // macronode 2: op2 ( , displacement_p)
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::ShearXInvariant<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1}, std::vector<float>{0.5f});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{0.0f, displacement_p.p()});
    CheckOperation<bosim::operation::ShearXInvariant<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3}, std::vector<float>{0.5f});
    // macronode 3: op3
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::ShearPInvariant<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1}, std::vector<float>{0.25f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::ShearPInvariant<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0}, std::vector<float>{0.25f});
    // macronode 4: op4
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Squeezing<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{0.5f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Squeezing<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{0.5f * std::numbers::pi_v<float>});
    // macronode 5: op5
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Squeezing45<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{0.25f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{3},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Squeezing45<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{0.25f * std::numbers::pi_v<float>});
    // macronode 6: op17
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{3},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{0.125f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{expected_measurement_squeezing_level,
                           -0.375f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{0.125f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{expected_measurement_squeezing_level,
                           -0.375f * std::numbers::pi_v<float>});
    // macronode 7: op14  (displacement_p, displacement_x)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{0.0f, displacement_p.p()});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{displacement_x.x(), 0.0f});
    // macronode 8: op18  (displacement_xp, displacement_p)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{displacement_xp.x(), displacement_xp.p()});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{-0.125f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{expected_measurement_squeezing_level,
                           -0.625f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{0.0f, displacement_p.p()});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{-0.125f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{expected_measurement_squeezing_level,
                           -0.625f * std::numbers::pi_v<float>});
    // macronode 9: op6 (displacement_xp, displacement_xp)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{displacement_xp.x(), displacement_xp.p()});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{displacement_xp.x(), displacement_xp.p()});
    CheckOperation<bosim::operation::ControlledZ<float>>(circuit_ops[circuit_op_index++],
                                                         std::vector<std::size_t>{1, 0},
                                                         std::vector<float>{0.125f});
    // macronode 10: op7 (displacement_x, displacement_p)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{displacement_x.x(), 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{0.0f, displacement_p.p()});
    CheckOperation<bosim::operation::BeamSplitter<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0, 3},
        std::vector<float>{1.0f, 0.125f * std::numbers::pi_v<float>});
    // macronode 11: op8
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::TwoModeShear<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0, 2},
                                                          std::vector<float>{2.0f, 3.0f});
    // macronode 12: op9
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Manual<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2, 1},
        std::vector<float>{-0.25f * std::numbers::pi_v<float>, 0.0f,
                           -0.25f * std::numbers::pi_v<float>, 0.0f});
    // macronode 13: op10 (displacement_p, displacement_xp)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{0.0f, displacement_p.p()});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{displacement_xp.x(), displacement_xp.p()});
    CheckOperation<bosim::operation::Manual<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2, 3},
        std::vector<float>{0.5f * std::numbers::pi_v<float>, 0.0f, 0.0f,
                           0.5f * std::numbers::pi_v<float>});
    // macronode 14: op15
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{3},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    // macronode 15
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    // macronode 16: op16
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    // macronode 17: op11 (displacement_xp, displacement_x)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2},
        std::vector<float>{displacement_xp.x(), displacement_xp.p()});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{displacement_x.x(), 0.0f});
    CheckOperation<bosim::operation::Manual<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2, 3},
        std::vector<float>{0.375f * std::numbers::pi_v<float>, -0.125f * std::numbers::pi_v<float>,
                           0.375f * std::numbers::pi_v<float>,
                           -0.125f * std::numbers::pi_v<float>});
    // macronode 18: op12
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Manual<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2, 1},
        std::vector<float>{-0.5f * std::numbers::pi_v<float>, 0.0f,
                           -0.5f * std::numbers::pi_v<float>, 0.0f});
    // macronode 19
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    // macronode 20
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{3},
                                                          std::vector<float>{0.0f, 0.0f});
    // macronode 21: op13
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{2},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::Manual<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2, 1},
        std::vector<float>{0.0f, 0.5f * std::numbers::pi_v<float>, 0.5f * std::numbers::pi_v<float>,
                           0.0f});
    // macronode 22: op19 (displacement_p, displacement_x)
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{0.0f, displacement_p.p()});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{0.25f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1},
        std::vector<float>{expected_measurement_squeezing_level,
                           -0.25f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::Displacement<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{displacement_x.x(), 0.0f});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{0.25f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{expected_measurement_squeezing_level,
                           -0.25f * std::numbers::pi_v<float>});
    // macronode 23: op20
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{0.5f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0},
        std::vector<float>{expected_measurement_squeezing_level, 0.0f});
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{3},
                                                          std::vector<float>{0.0f, 0.0f});
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{0.5f * std::numbers::pi_v<float>});
    CheckOperation<bosim::operation::util::ReplaceBySqueezedState<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3},
        std::vector<float>{expected_measurement_squeezing_level, 0.0f});

    EXPECT_EQ(circuit_ops.size(), circuit_op_index);
}

template <std::floating_point Float>
void CheckFFFuncsArbitrary(const std::array<bosim::FFFunc<Float>, 4>& ff_funcs,
                           const bool is_arbitrary_first, const bool is_swap,
                           const std::vector<Float>& args) {
    constexpr auto NumFFFuncs = std::size_t{4};
    const auto expected_results = [is_arbitrary_first, is_swap]() -> std::array<Float, NumFFFuncs> {
        if (is_arbitrary_first) {
            constexpr auto ExpectedArbitraryFirstThetaMinus = 0.0f;
            constexpr auto ExpectedArbitraryFirstThetaPlus = 0.5f * std::numbers::pi_v<float>;
            if (is_swap) {
                return {ExpectedArbitraryFirstThetaPlus, ExpectedArbitraryFirstThetaMinus,
                        ExpectedArbitraryFirstThetaMinus, ExpectedArbitraryFirstThetaPlus};
            } else {
                return {ExpectedArbitraryFirstThetaMinus, ExpectedArbitraryFirstThetaPlus,
                        ExpectedArbitraryFirstThetaMinus, ExpectedArbitraryFirstThetaPlus};
            }
        } else {
            constexpr auto ExpectedArbitrarySecondThetaMinus = -0.125f * std::numbers::pi_v<float>;
            constexpr auto ExpectedArbitrarySecondThetaPlus = 0.375f * std::numbers::pi_v<float>;
            if (is_swap) {
                return {ExpectedArbitrarySecondThetaMinus, ExpectedArbitrarySecondThetaPlus,
                        ExpectedArbitrarySecondThetaPlus, ExpectedArbitrarySecondThetaMinus};
            } else {
                return {ExpectedArbitrarySecondThetaPlus, ExpectedArbitrarySecondThetaMinus,
                        ExpectedArbitrarySecondThetaPlus, ExpectedArbitrarySecondThetaMinus};
            }
        }
    }();

    const auto ff_func_results = std::array<Float, NumFFFuncs>{
        ff_funcs[0](args), ff_funcs[1](args), ff_funcs[2](args), ff_funcs[3](args)};
    EXPECT_EQ(expected_results, ff_func_results);
}

TEST(GraphRepresentation, ConstructFFFuncsToManual) {
    const auto env = bosim::python::PythonEnvironment();

    constexpr auto NumParamsOfArbitrary = std::size_t{3};

    const auto ff_func_alpha =
        bosim::FFFunc<float>{[](const std::vector<float>& args) { return 0.5f * args.front(); }};
    const auto ff_func_beta =
        bosim::FFFunc<float>{[](const std::vector<float>& args) { return 0.25f * args.front(); }};
    const auto ff_func_lambda =
        bosim::FFFunc<float>{[](const std::vector<float>& args) { return args.front(); }};

    // Target: ArbitraryFirst beta
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                std::nullopt, ff_func_beta, std::nullopt};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{1};
        const auto initial_params_of_arbitrary =
            std::vector{0.5f * std::numbers::pi_v<float>, 0.0f, 0.0f};
        constexpr auto IsArbitraryFirst = true;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_through, IsArbitraryFirst, false,
                              std::vector{std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_swap, IsArbitraryFirst, true,
                              std::vector{std::numbers::pi_v<float>});
    }
    // Target: ArbitraryFirst lambda
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                std::nullopt, std::nullopt, ff_func_lambda};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{2};
        const auto initial_params_of_arbitrary =
            std::vector{0.5f * std::numbers::pi_v<float>, 0.25f * std::numbers::pi_v<float>, 1.0f};
        constexpr auto IsArbitraryFirst = true;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_through, IsArbitraryFirst, false,
                              std::vector{0.0f});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_swap, IsArbitraryFirst, true, std::vector{0.0f});
    }
    // Target: ArbitraryFirst lambda, beta
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                std::nullopt, ff_func_beta, ff_func_lambda};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{2, 1};
        const auto initial_params_of_arbitrary =
            std::vector{0.5f * std::numbers::pi_v<float>, 0.0f, 1.0f};
        constexpr auto IsArbitraryFirst = true;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_through, IsArbitraryFirst, false,
                              std::vector{0.0f, std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_swap, IsArbitraryFirst, true,
                              std::vector{0.0f, std::numbers::pi_v<float>});
    }
    // Target: ArbitraryFirst lambda, alpha, beta
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                ff_func_alpha, ff_func_beta, ff_func_lambda};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{2, 0, 1};
        const auto initial_params_of_arbitrary = std::vector{0.0f, 0.0f, 1.0f};
        constexpr auto IsArbitraryFirst = true;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(
            ff_funcs_to_manual_through, IsArbitraryFirst, false,
            std::vector{0.0f, std::numbers::pi_v<float>, std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(
            ff_funcs_to_manual_swap, IsArbitraryFirst, true,
            std::vector{0.0f, std::numbers::pi_v<float>, std::numbers::pi_v<float>});
    }

    // Target: ArbitrarySecond alpha
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                ff_func_alpha, std::nullopt, std::nullopt};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{0};
        const auto initial_params_of_arbitrary =
            std::vector{0.0f, 0.25f * std::numbers::pi_v<float>, 0.0f};
        constexpr auto IsArbitraryFirst = false;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_through, IsArbitraryFirst, false,
                              std::vector{std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_swap, IsArbitraryFirst, true,
                              std::vector{std::numbers::pi_v<float>});
    }
    // Target: ArbitrarySecond beta
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                std::nullopt, ff_func_beta, std::nullopt};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{1};
        const auto initial_params_of_arbitrary =
            std::vector{0.5f * std::numbers::pi_v<float>, 0.0f, 0.0f};
        constexpr auto IsArbitraryFirst = false;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_through, IsArbitraryFirst, false,
                              std::vector{std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_swap, IsArbitraryFirst, true,
                              std::vector{std::numbers::pi_v<float>});
    }
    // Target: ArbitrarySecond beta, alpha
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                ff_func_alpha, ff_func_beta, std::nullopt};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{1, 0};
        const auto initial_params_of_arbitrary = std::vector{0.0f, 0.0f, 0.0f};
        constexpr auto IsArbitraryFirst = false;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_through, IsArbitraryFirst, false,
                              std::vector{std::numbers::pi_v<float>, std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(ff_funcs_to_manual_swap, IsArbitraryFirst, true,
                              std::vector{std::numbers::pi_v<float>, std::numbers::pi_v<float>});
    }
    // Target: ArbitrarySecond beta, lambda, alpha
    {
        const auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<float>>, NumParamsOfArbitrary>{
                ff_func_alpha, ff_func_beta, ff_func_lambda};
        const auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{1, 2, 0};
        const auto initial_params_of_arbitrary = std::vector{0.0f, 0.0f, 1.0f};
        constexpr auto IsArbitraryFirst = false;

        const auto ff_funcs_to_manual_through = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, false);
        CheckFFFuncsArbitrary(
            ff_funcs_to_manual_through, IsArbitraryFirst, false,
            std::vector{std::numbers::pi_v<float>, 0.0f, std::numbers::pi_v<float>});

        const auto ff_funcs_to_manual_swap = bosim::graph::ConstructFFFuncsToManual(
            ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
            IsArbitraryFirst, true);
        CheckFFFuncsArbitrary(
            ff_funcs_to_manual_swap, IsArbitraryFirst, true,
            std::vector{std::numbers::pi_v<float>, 0.0f, std::numbers::pi_v<float>});
    }
}

TEST(GraphRepresentation, GraphCircuitFF) {
    using namespace mqc3_cloud::program::v1;
    const auto env = bosim::python::PythonEnvironment();

    // Construct a graph representation.
    GraphRepresentation graph_repr;
    graph_repr.set_n_local_macronodes(10);
    graph_repr.set_n_steps(2);

    // Create displacement objects for testing.
    GraphOperation::Displacement displacement;
    displacement.set_x(-1.0f);
    displacement.set_p(0.5f);

    // Add operations to the graph representation.
    // op1: 0 [1] 2 3 [4] 5
    //  (These numbers are operation indices in circuit representation. [] represents measurement.)
    AddOperationToGraphRepr(graph_repr, 0, false, std::vector{0.5f * std::numbers::pi_v<float>},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, std::nullopt, std::nullopt,
                            true);
    // op2: 6 7
    AddOperationToGraphRepr(graph_repr, 1, true, std::vector<float>{},
                            GraphOperation::OPERATION_TYPE_WIRING, displacement, std::nullopt);
    // op3: 8 9 10 11
    AddOperationToGraphRepr(graph_repr, 2, true, std::vector{0.0f},
                            GraphOperation::OPERATION_TYPE_PHASE_ROTATION, std::nullopt,
                            displacement);
    // op4: 12 13 14 15
    AddOperationToGraphRepr(graph_repr, 3, true, std::vector{0.0f},
                            GraphOperation::OPERATION_TYPE_SHEAR_X_INVARIANT, std::nullopt,
                            std::nullopt);
    // op5: 16 17 18 19
    AddOperationToGraphRepr(graph_repr, 4, false, std::vector{0.0f},
                            GraphOperation::OPERATION_TYPE_SHEAR_P_INVARIANT, std::nullopt,
                            std::nullopt);
    // op6: 20 21 22 23
    AddOperationToGraphRepr(graph_repr, 5, true, std::vector{1.0f},
                            GraphOperation::OPERATION_TYPE_SQUEEZING, std::nullopt, std::nullopt);
    // op7: 24 25 26 27
    AddOperationToGraphRepr(graph_repr, 6, false, std::vector{1.0f},
                            GraphOperation::OPERATION_TYPE_SQUEEZING_45, std::nullopt,
                            std::nullopt);
    // op8: 28 [29] 30 31 [32] 33
    AddOperationToGraphRepr(graph_repr, 7, false, std::vector{0.0f},
                            GraphOperation::OPERATION_TYPE_MEASUREMENT, std::nullopt, std::nullopt,
                            true);
    // op9: 34 35 36
    AddOperationToGraphRepr(graph_repr, 8, true, std::vector{0.0f},
                            GraphOperation::OPERATION_TYPE_CONTROLLED_Z, std::nullopt,
                            std::nullopt);
    // op10: 37 38 39
    AddOperationToGraphRepr(graph_repr, 9, false, std::vector{0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_BEAM_SPLITTER, std::nullopt,
                            std::nullopt);
    // op11: 40 41 42
    AddOperationToGraphRepr(graph_repr, 10, true, std::vector{0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_TWO_MODE_SHEAR, std::nullopt,
                            std::nullopt);
    // op12: 43 44 45
    AddOperationToGraphRepr(graph_repr, 11, false, std::vector{0.0f, 1.0f, 2.0f, 3.0f},
                            GraphOperation::OPERATION_TYPE_MANUAL, std::nullopt, std::nullopt);
    // op13: 46 [47] 48 49 [50] 51
    AddOperationToGraphRepr(graph_repr, 12, true, std::vector{0.0f},
                            GraphOperation::OPERATION_TYPE_INITIALIZATION, displacement,
                            displacement, true);
    // op14: 52 53 54
    AddOperationToGraphRepr(graph_repr, 13, false, std::vector{0.0f, 0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST, displacement,
                            displacement);
    // op15: 55 56 57
    AddOperationToGraphRepr(graph_repr, 14, true, std::vector{0.0f, 0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND, displacement,
                            displacement);
    // op16: 58 59 60
    AddOperationToGraphRepr(graph_repr, 15, true, std::vector{0.0f, 0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST, displacement,
                            displacement);
    // op17: 61 62 63
    AddOperationToGraphRepr(graph_repr, 16, false, std::vector{0.0f, 0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND, displacement,
                            displacement);
    // op18: 64 65 66
    AddOperationToGraphRepr(graph_repr, 17, false, std::vector{0.0f, 0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST, std::nullopt,
                            displacement);
    // op19: 67 68 69
    AddOperationToGraphRepr(graph_repr, 18, true, std::vector{0.0f, 0.0f, 0.0f},
                            GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND, displacement,
                            std::nullopt);

    // Non-linear feedforward functions.
    auto* func = graph_repr.add_functions();
    func->add_code(R"(def func(x):
        return x * x)");

    const auto func_mappings =
        std::vector<std::pair<std::vector<float>, float>>{{std::vector{0.0f}, 0.0f},
                                                          {std::vector{-1.0f}, 1.0f},
                                                          {std::vector{1.0f}, 1.0f},
                                                          {std::vector{2.0f}, 4.0f},
                                                          {std::vector{-5.0f}, 25.0f}};
    const auto replace_by_squeezed_func_mappings =
        std::vector<std::pair<std::vector<float>, float>>{
            {std::vector{0.0f}, -0.5f * std::numbers::pi_v<float>},
            {std::vector{-1.0f}, 1.0f - 0.5f * std::numbers::pi_v<float>},
            {std::vector{1.0f}, 1.0f - 0.5f * std::numbers::pi_v<float>},
            {std::vector{2.0f}, 4.0f - 0.5f * std::numbers::pi_v<float>},
            {std::vector{-5.0f}, 25.0f - 0.5f * std::numbers::pi_v<float>}};

    const auto manual_minus_func_mappings_two_inputs =
        std::vector<std::pair<std::vector<float>, float>>{
            {std::vector{0.0f, 0.0f}, -0.25f * std::numbers::pi_v<float>}};
    const auto manual_minus_func_mappings_three_inputs =
        std::vector<std::pair<std::vector<float>, float>>{
            {std::vector{0.0f, 0.0f, 0.0f}, -0.25f * std::numbers::pi_v<float>}};
    const auto manual_plus_func_mappings_two_inputs =
        std::vector<std::pair<std::vector<float>, float>>{
            {std::vector{0.0f, 0.0f}, 0.25f * std::numbers::pi_v<float>}};
    const auto manual_plus_func_mappings_three_inputs =
        std::vector<std::pair<std::vector<float>, float>>{
            {std::vector{0.0f, 0.0f, 0.0f}, 0.25f * std::numbers::pi_v<float>}};

    // Add non-linear feedforward.
    // 1 -> 6 (`x` of `displacement_k_minus_1`)
    AddFFToGraphRepr(graph_repr, 0, 0, 1, 4, 0);
    // 4 -> 6 (`p` of `displacement_k_minus_1`)
    AddFFToGraphRepr(graph_repr, 0, 1, 1, 5, 0);
    // 4 -> 10 (`x` of `displacement_k_minus_n`)
    AddFFToGraphRepr(graph_repr, 0, 1, 2, 6, 0);
    // 1 -> 10 (`p` of `displacement_k_minus_n`)
    AddFFToGraphRepr(graph_repr, 0, 0, 2, 7, 0);
    // 1 -> 9, 11
    AddFFToGraphRepr(graph_repr, 0, 0, 2, 0, 0);
    // 4 -> 13, 15
    AddFFToGraphRepr(graph_repr, 0, 1, 3, 0, 0);
    // 1 -> 17, 19
    AddFFToGraphRepr(graph_repr, 0, 0, 4, 0, 0);
    // 4 -> 21, 23
    AddFFToGraphRepr(graph_repr, 0, 1, 5, 0, 0);
    // 1 -> 25, 27
    AddFFToGraphRepr(graph_repr, 0, 0, 6, 0, 0);
    // 29 -> 36
    AddFFToGraphRepr(graph_repr, 7, 0, 8, 0, 0);
    // 29 -> 39
    AddFFToGraphRepr(graph_repr, 7, 0, 9, 1, 0);
    // 32 -> 39
    AddFFToGraphRepr(graph_repr, 7, 1, 9, 0, 0);
    // 29 -> 42
    AddFFToGraphRepr(graph_repr, 7, 0, 10, 0, 0);
    // 32 -> 42
    AddFFToGraphRepr(graph_repr, 7, 1, 10, 1, 0);
    // 1 -> 45
    AddFFToGraphRepr(graph_repr, 0, 0, 11, 3, 0);
    // 4 -> 45
    AddFFToGraphRepr(graph_repr, 0, 1, 11, 2, 0);
    // 29 -> 45
    AddFFToGraphRepr(graph_repr, 7, 0, 11, 1, 0);
    // 32 -> 45
    AddFFToGraphRepr(graph_repr, 7, 1, 11, 0, 0);
    // 1 -> 29, 30, 32, 33
    AddFFToGraphRepr(graph_repr, 0, 0, 7, 0, 0);
    // 4 -> 47, 48, 50, 51
    AddFFToGraphRepr(graph_repr, 0, 1, 12, 0, 0);

    // Displacement of Arbitrary operations
    // 47 -> 52 (`x` of `displacement_k_minus_1`)
    AddFFToGraphRepr(graph_repr, 12, 0, 13, 4, 0);
    // 50 -> 53 (`x` of `displacement_k_minus_n`)
    AddFFToGraphRepr(graph_repr, 12, 1, 13, 6, 0);
    // 47 -> 55 (`p` of `displacement_k_minus_1`)
    AddFFToGraphRepr(graph_repr, 12, 0, 14, 5, 0);
    // 50 -> 56 (`p` of `displacement_k_minus_n`)
    AddFFToGraphRepr(graph_repr, 12, 1, 14, 7, 0);

    // Arbitrary operations
    // No FFs will be added for the Manual operation of macronode 13.
    AddFFToGraphRepr(graph_repr, 0, 0, 13, 0, 0);

    // No FFs will be added for the Manual operation of macronode 14.
    AddFFToGraphRepr(graph_repr, 12, 1, 14, 2, 0);

    // Four FFs will be added for the Manual operation of macronode 15.
    // 50 -> 60 (alpha)
    AddFFToGraphRepr(graph_repr, 12, 1, 15, 0, 0);
    // 1 -> 60 (lambda)
    AddFFToGraphRepr(graph_repr, 0, 0, 15, 2, 0);

    // Four FFs will be added for the Manual operation of macronode 16.
    // 4 -> 63 (beta)
    AddFFToGraphRepr(graph_repr, 0, 1, 16, 1, 0);
    // 47 -> 63 (alpha)
    AddFFToGraphRepr(graph_repr, 12, 0, 16, 0, 0);

    // Four FFs will be added for the Manual operation of macronode 17.
    // 1 -> 66 (beta)
    AddFFToGraphRepr(graph_repr, 0, 0, 17, 1, 0);
    // 4 -> 66 (alpha)
    AddFFToGraphRepr(graph_repr, 0, 1, 17, 0, 0);
    // 29 -> 66 (lambda)
    AddFFToGraphRepr(graph_repr, 7, 0, 17, 2, 0);

    // Four FFs will be added for the Manual operation of macronode 18.
    // 32 -> 69 (beta)
    AddFFToGraphRepr(graph_repr, 7, 1, 18, 1, 0);
    // 4 -> 69 (lambda)
    AddFFToGraphRepr(graph_repr, 0, 1, 18, 2, 0);
    // 1 -> 69 (alpha)
    AddFFToGraphRepr(graph_repr, 0, 0, 18, 0, 0);

    // Convert the graph representation to a circuit object.
    constexpr auto SqueezingLevel = 10.0f;
    auto circuit = bosim::graph::GraphCircuit<float>(graph_repr, SqueezingLevel);
    const auto& circuit_ops = circuit.GetMutOperations();
    EXPECT_EQ(72, circuit_ops.size());

    const auto& nlffs = circuit.GetFFFuncs();
    EXPECT_EQ(51, nlffs.size());

    CheckFeedForwardParams(nlffs, 1, 13,
                           std::vector<bosim::ParamIdx>({{6, 0},
                                                         {9, 0},
                                                         {10, 1},
                                                         {11, 0},
                                                         {17, 0},
                                                         {19, 0},
                                                         {25, 0},
                                                         {27, 0},
                                                         {29, 0},
                                                         {30, 1},
                                                         {32, 0},
                                                         {33, 1},
                                                         {45, 3}}));
    CheckFeedForwardParams(nlffs, 4, 11,
                           std::vector<bosim::ParamIdx>({{6, 1},
                                                         {10, 0},
                                                         {13, 0},
                                                         {15, 0},
                                                         {21, 0},
                                                         {23, 0},
                                                         {45, 2},
                                                         {47, 0},
                                                         {48, 1},
                                                         {50, 0},
                                                         {51, 1}}));

    CheckFeedForwardParams(
        nlffs, 29, 8,
        std::vector<bosim::ParamIdx>(
            {{36, 0}, {39, 1}, {42, 0}, {45, 1}, {66, 0}, {66, 1}, {66, 2}, {66, 3}}));
    CheckFeedForwardParams(nlffs, 32, 7,
                           std::vector<bosim::ParamIdx>(
                               {{39, 0}, {42, 1}, {45, 0}, {69, 0}, {69, 1}, {69, 2}, {69, 3}}));

    CheckFeedForwardParams(
        nlffs, 47, 6,
        std::vector<bosim::ParamIdx>({{52, 0}, {55, 1}, {63, 0}, {63, 1}, {63, 2}, {63, 3}}));
    CheckFeedForwardParams(
        nlffs, 50, 6,
        std::vector<bosim::ParamIdx>({{53, 0}, {56, 1}, {60, 0}, {60, 1}, {60, 2}, {60, 3}}));

    // Check feedforward functions
    for (const auto& [_, ff] : nlffs) {
        switch (ff->target_param.op_idx) {
            case 30:
            case 33:
            case 48:
            case 51:
                // `ReplaceBySqueezedState` operations
                EXPECT_EQ(1, ff->target_param.param_idx);
                CheckFeedForwardFunction(replace_by_squeezed_func_mappings, ff->func);
                break;
            case 60:
                // `Manual` operation corresponding to ArbitraryFirst (swap)
                switch (ff->target_param.param_idx) {
                    case 0:
                    case 3:
                        CheckFeedForwardFunction(manual_plus_func_mappings_two_inputs, ff->func);
                        break;
                    case 1:
                    case 2:
                        CheckFeedForwardFunction(manual_minus_func_mappings_two_inputs, ff->func);
                        break;
                    default: FAIL(); break;
                }
                break;
            case 63:
                // `Manual` operation corresponding to ArbitrarySecond (through)
                switch (ff->target_param.param_idx) {
                    case 0:
                    case 2:
                        CheckFeedForwardFunction(manual_plus_func_mappings_two_inputs, ff->func);
                        break;
                    case 1:
                    case 3:
                        CheckFeedForwardFunction(manual_minus_func_mappings_two_inputs, ff->func);
                        break;
                    default: FAIL(); break;
                }
                break;
            case 54:
            case 66:
                // `Manual` operation corresponding to ArbitraryFirst (through)
                switch (ff->target_param.param_idx) {
                    case 1:
                    case 3:
                        CheckFeedForwardFunction(manual_plus_func_mappings_three_inputs, ff->func);
                        break;
                    case 0:
                    case 2:
                        CheckFeedForwardFunction(manual_minus_func_mappings_three_inputs, ff->func);
                        break;
                    default: FAIL(); break;
                }
                break;
            case 57:
            case 69:
                // `Manual` operation corresponding to ArbitrarySecond (swap)
                switch (ff->target_param.param_idx) {
                    case 1:
                    case 2:
                        CheckFeedForwardFunction(manual_plus_func_mappings_three_inputs, ff->func);
                        break;
                    case 0:
                    case 3:
                        CheckFeedForwardFunction(manual_minus_func_mappings_three_inputs, ff->func);
                        break;
                    default: FAIL(); break;
                }
                break;
            default: CheckFeedForwardFunction(func_mappings, ff->func); break;
        }
    }
}
