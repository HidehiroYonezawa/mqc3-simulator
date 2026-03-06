#include "bosim/simulate/circuit_repr.h"

#include <gtest/gtest.h>

#include "circuit_repr.h"
#include "mqc3_cloud/program/v1/circuit.pb.h"

auto CircuitForTest() -> mqc3_cloud::program::v1::CircuitRepresentation {
    using namespace mqc3_cloud::program::v1;
    using PbCircuitRepresentation = CircuitRepresentation;
    using PbCircuitOperation = CircuitOperation;

    // Construct a circuit representation.
    PbCircuitRepresentation pb_circuit_repr;
    pb_circuit_repr.set_n_modes(4);

    // Add operations to the graph representation.
    // op0
    AddOperationToPbCircuitRepr<double>(pb_circuit_repr,
                                        PbCircuitOperation::OPERATION_TYPE_MEASUREMENT, {3}, {2.0});
    // op1
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_DISPLACEMENT, {0}, {1.0, 2.0});
    // op2
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_PHASE_ROTATION, {1}, {3.0});
    // op3
    AddOperationToPbCircuitRepr<double>(pb_circuit_repr,
                                        PbCircuitOperation::OPERATION_TYPE_SQUEEZING, {2}, {4.0});
    // op4
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_SQUEEZING_45, {1}, {5.0});
    // op5
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_SHEAR_X_INVARIANT, {0}, {6.0});
    // op6
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_SHEAR_P_INVARIANT, {1}, {7.0});
    // op7
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_ARBITRARY, {2}, {8.0, 9.0, 1.0});
    // op8
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_BEAM_SPLITTER, {1, 0}, {0.2, 3.0});
    // op9
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_CONTROLLED_Z, {1, 2}, {4.0});
    // op10
    AddOperationToPbCircuitRepr<double>(
        pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_TWO_MODE_SHEAR, {1, 0}, {5.0, 6.0});
    // op11
    AddOperationToPbCircuitRepr<double>(pb_circuit_repr, PbCircuitOperation::OPERATION_TYPE_MANUAL,
                                        {1, 2}, {7.0, 8.0, 9.0, 1.0});

    return pb_circuit_repr;
}

TEST(CircuitRepresentation, CircuitFromProto) {
    using namespace mqc3_cloud::program::v1;

    // Convert the graph representation to a circuit object.
    auto pb_circuit_repr = CircuitForTest();
    auto circuit = bosim::circuit::CircuitFromProto<float>(&pb_circuit_repr);
    const auto& circuit_ops = circuit.GetMutOperations();
    EXPECT_EQ(12, circuit_ops.size());

    // Check Operation types, parameters, and target modes.
    auto circuit_op_index = std::size_t{0};
    // op0
    CheckOperation<bosim::operation::HomodyneSingleMode<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{3}, std::vector<float>{2.0f});
    // op1
    CheckOperation<bosim::operation::Displacement<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{0},
                                                          std::vector<float>{1.0f, 2.0f});
    // op2
    CheckOperation<bosim::operation::PhaseRotation<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1}, std::vector<float>{3.0f});
    // op3
    CheckOperation<bosim::operation::Squeezing<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{2}, std::vector<float>{4.0f});
    // op4
    CheckOperation<bosim::operation::Squeezing45<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1}, std::vector<float>{5.0f});
    // op5
    CheckOperation<bosim::operation::ShearXInvariant<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{0}, std::vector<float>{6.0f});
    // op6
    CheckOperation<bosim::operation::ShearPInvariant<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1}, std::vector<float>{7.0f});
    // op7
    CheckOperation<bosim::operation::Arbitrary<float>>(circuit_ops[circuit_op_index++],
                                                       std::vector<std::size_t>{2},
                                                       std::vector<float>{8.0f, 9.0f, 1.0f});
    // op8
    CheckOperation<bosim::operation::BeamSplitter<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1, 0},
                                                          std::vector<float>{0.2f, 3.0f});
    // op9
    CheckOperation<bosim::operation::ControlledZ<float>>(
        circuit_ops[circuit_op_index++], std::vector<std::size_t>{1, 2}, std::vector<float>{4.0f});
    // op10
    CheckOperation<bosim::operation::TwoModeShear<float>>(circuit_ops[circuit_op_index++],
                                                          std::vector<std::size_t>{1, 0},
                                                          std::vector<float>{5.0f, 6.0f});
    // op11
    CheckOperation<bosim::operation::Manual<float>>(circuit_ops[circuit_op_index++],
                                                    std::vector<std::size_t>{1, 2},
                                                    std::vector<float>{7.0f, 8.0f, 9.0f, 1.0f});

    EXPECT_EQ(circuit_ops.size(), circuit_op_index);
}

TEST(CircuitRepresentation, CircuitFF) {
    using namespace mqc3_cloud::program::v1;
    const auto env = bosim::python::PythonEnvironment();

    // Construct a graph representation.
    auto pb_circuit_repr = CircuitForTest();

    // Non-linear feedforward functions.
    auto* func = pb_circuit_repr.add_functions();
    func->add_code(R"(def func(x):
        return x * x)");

    // Add non-linear feedforward.
    AddFFToPbCircuitRepr(pb_circuit_repr, 0, 0, 1, 1);
    AddFFToPbCircuitRepr(pb_circuit_repr, 0, 0, 4, 0);
    AddFFToPbCircuitRepr(pb_circuit_repr, 0, 0, 7, 2);
    AddFFToPbCircuitRepr(pb_circuit_repr, 0, 0, 8, 0);
    AddFFToPbCircuitRepr(pb_circuit_repr, 0, 0, 11, 1);
    AddFFToPbCircuitRepr(pb_circuit_repr, 0, 0, 11, 3);

    auto circuit = bosim::circuit::CircuitFromProto<float>(&pb_circuit_repr);
    const auto& circuit_ops = circuit.GetMutOperations();
    EXPECT_EQ(12, circuit_ops.size());

    const auto& nlffs = circuit.GetFFFuncs();
    EXPECT_EQ(6, nlffs.size());

    CheckFeedForwardParams(
        nlffs, 0, 6,
        std::vector<bosim::ParamIdx>({{1, 1}, {4, 0}, {7, 2}, {8, 0}, {11, 1}, {11, 3}}));
    const auto func_mappings =
        std::vector<std::pair<std::vector<float>, float>>{{std::vector{0.0f}, 0.0f},
                                                          {std::vector{-1.0f}, 1.0f},
                                                          {std::vector{1.0f}, 1.0f},
                                                          {std::vector{2.0f}, 4.0f},
                                                          {std::vector{-5.0f}, 25.0f}};
    for (const auto& [_, ff] : nlffs) { CheckFeedForwardFunction(func_mappings, ff->func); }
}
