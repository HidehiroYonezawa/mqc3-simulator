#pragma once

#include "bosim/base/timer.h"
#include "bosim/circuit.h"
#include "bosim/python/function.h"

#include "mqc3_cloud/program/v1/graph.pb.h"

namespace bosim::graph {

using mqc3_cloud::program::v1::GraphFF;
using mqc3_cloud::program::v1::GraphOperation;
using mqc3_cloud::program::v1::GraphRepresentation;

/**
 * @brief Calculate the parameters of Manual operation corresponding to ArbitraryFirst operation.
 *
 * @tparam Float The floating point type.
 * @param params_of_arbitrary Parameters of ArbitraryFirst operation.
 * @param is_swap Whether the ArbitraryFirst operation is swap or not.
 * @return Parameters of Manual operation.
 */
template <std::floating_point Float>
std::vector<Float> CalcParamsFromArbitraryFirst(const std::vector<Float>& params_of_arbitrary,
                                                const bool is_swap) {
    if (params_of_arbitrary.size() != 3) {
        throw std::invalid_argument("Size of 'params_of_arbitrary' must be 3");
    }
    const auto [alpha, beta, lambda] =
        std::tie(params_of_arbitrary[0], params_of_arbitrary[1], params_of_arbitrary[2]);
    const auto atan_exp_l = std::atan(std::exp(lambda));
    const auto minus = beta - atan_exp_l;
    const auto plus = beta + atan_exp_l;
    if (is_swap) {
        return {plus, minus, minus, plus};
    } else {
        return {minus, plus, minus, plus};
    }
}

/**
 * @brief Calculate the parameters of Manual operation corresponding to ArbitrarySecond operation.
 *
 * @tparam Float The floating point type.
 * @param params_of_arbitrary Parameters of ArbitrarySecond operation.
 * @param is_swap Whether the ArbitrarySecond operation is swap or not.
 * @return Parameters of Manual operation.
 */
template <std::floating_point Float>
std::vector<Float> CalcParamsFromArbitrarySecond(const std::vector<Float>& params_of_arbitrary,
                                                 const bool is_swap) {
    if (params_of_arbitrary.size() != 3) {
        throw std::invalid_argument("Size of 'params_of_arbitrary' must be 3");
    }
    const auto [alpha, beta, lambda] =
        std::tie(params_of_arbitrary[0], params_of_arbitrary[1], params_of_arbitrary[2]);
    const auto h_a_b = Float{0.5} * (alpha - beta);
    const auto minus = h_a_b - Float{0.25} * std::numbers::pi_v<Float>;
    const auto plus = h_a_b + Float{0.25} * std::numbers::pi_v<Float>;
    if (is_swap) {
        return {minus, plus, plus, minus};
    } else {
        return {plus, minus, plus, minus};
    }
}

/**
 * @brief Calculate the parameters of the circuit operations corresponding to the given graph
 * operation.
 *
 * @tparam Float The floating point type.
 * @param graph_op Graph operation.
 * @return Parameters of the circuit operations.
 */
template <std::floating_point Float>
std::vector<Float> CalcCircuitOpParams(const GraphOperation& graph_op) {
    const auto graph_op_params =
        std::vector<Float>(graph_op.parameters().begin(), graph_op.parameters().end());
    if (graph_op.type() == GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST) {
        return CalcParamsFromArbitraryFirst(graph_op_params, graph_op.swap());
    }
    if (graph_op.type() == GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND) {
        return CalcParamsFromArbitrarySecond(graph_op_params, graph_op.swap());
    }
    return graph_op_params;
}

/**
 * @brief Calculate the parameters of the displacement from the upper macronode.
 *
 * @tparam Float The floating point type.
 * @param graph_op Graph operation.
 * @return Parameters of the displacement operation.
 */
template <std::floating_point Float>
std::vector<Float> CalcUpDisplacementParams(const GraphOperation& graph_op) {
    if (graph_op.has_displacement_k_minus_1()) {
        return std::vector<Float>{graph_op.displacement_k_minus_1().x(),
                                  graph_op.displacement_k_minus_1().p()};
    }
    return std::vector<Float>{0, 0};
}

/**
 * @brief Calculate the parameters of the displacement from the left macronode.
 *
 * @tparam Float The floating point type.
 * @param graph_op Graph operation.
 * @return Parameters of the displacement operation.
 */
template <std::floating_point Float>
std::vector<Float> CalcLeftDisplacementParams(const GraphOperation& graph_op) {
    if (graph_op.has_displacement_k_minus_n()) {
        return std::vector<Float>{graph_op.displacement_k_minus_n().x(),
                                  graph_op.displacement_k_minus_n().p()};
    }
    return std::vector<Float>{0, 0};
}

/**
 * @brief Add a displacement operation to the circuit.
 *
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the displacement operation to.
 * @param target_mode Target mode of the displacement operation.
 * @param params Parameters of the displacement operation.
 * @return Index of the added operation.
 */
template <std::floating_point Float>
std::size_t AddDisplacement(Circuit<Float>& circuit, const std::size_t target_mode,
                            const std::vector<Float>& params) {
    if (params.size() != 2) { throw std::invalid_argument("Size of 'params' must be 2"); }
    circuit.template EmplaceOperation<bosim::operation::Displacement<Float>>(
        std::vector{target_mode}, params);
    return circuit.GetMutOperations().size() - 1;
}

/**
 * @brief Structure representing circuit operations corresponding to a graph operation.
 */
struct CircuitOperations {
    enum class MacronodeOperationType {
        UNARY_OPERATION,
        BINARY_OPERATION,
        MEASUREMENT_OPERATION,
        DISPLACEMENT_ONLY_OPERATION
    };

    /**
     * @brief Macronode operation type.
     */
    MacronodeOperationType type;

    /**
     * @brief Operation index of the displacement from the upper macronode.
     */
    std::size_t up_displacement_index;

    /**
     * @brief Operation index of the displacement from the left macronode.
     */
    std::size_t left_displacement_index;

    /**
     * @brief Operation indices.
     */
    std::vector<std::size_t> op_indices;
};

using MacronodeToCircuitOps = std::unordered_map<std::uint32_t, CircuitOperations>;

/**
 * @brief Add unary operations to the circuit.
 *
 * @tparam Op Operation type.
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the operations to.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @param graph_op Graph operation.
 * @param up_mode Up mode of the graph operation.
 * @param left_mode Left mode of the graph operation.
 */
template <typename Op, std::floating_point Float>
void AddUnaryOperations(Circuit<Float>& circuit, MacronodeToCircuitOps& macronode_to_circuit_ops,
                        const GraphOperation& graph_op, const std::size_t up_mode,
                        const std::size_t left_mode) {
    const auto params = CalcCircuitOpParams<Float>(graph_op);
    const auto up_displacement_index =
        AddDisplacement(circuit, up_mode, CalcUpDisplacementParams<Float>(graph_op));
    circuit.template EmplaceOperation<Op>(std::vector{up_mode}, params);
    const auto first_unary_op_index = circuit.GetMutOperations().size() - 1;
    const auto left_displacement_index =
        AddDisplacement(circuit, left_mode, CalcLeftDisplacementParams<Float>(graph_op));
    circuit.template EmplaceOperation<Op>(std::vector{left_mode}, params);
    const auto seconde_unary_op_index = circuit.GetMutOperations().size() - 1;

    macronode_to_circuit_ops[graph_op.macronode()] = {
        .type = CircuitOperations::MacronodeOperationType::UNARY_OPERATION,
        .up_displacement_index = up_displacement_index,
        .left_displacement_index = left_displacement_index,
        .op_indices = {first_unary_op_index, seconde_unary_op_index}};
}

/*
 * @brief Add a binary operation to the circuit.
 *
 * @tparam Op Operation type.
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the operation to.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @param graph_op Graph operation.
 * @param up_mode Up mode of the graph operation.
 * @param left_mode Left mode of the graph operation.
 */
template <typename Op, std::floating_point Float>
void AddBinaryOperation(Circuit<Float>& circuit, MacronodeToCircuitOps& macronode_to_circuit_ops,
                        const GraphOperation& graph_op, const std::size_t up_mode,
                        const std::size_t left_mode) {
    const auto params = CalcCircuitOpParams<Float>(graph_op);
    const auto up_displacement_index =
        AddDisplacement(circuit, up_mode, CalcUpDisplacementParams<Float>(graph_op));
    const auto left_displacement_index =
        AddDisplacement(circuit, left_mode, CalcLeftDisplacementParams<Float>(graph_op));
    circuit.template EmplaceOperation<Op>(std::vector{up_mode, left_mode}, params);
    const auto binary_op_index = circuit.GetMutOperations().size() - 1;

    macronode_to_circuit_ops[graph_op.macronode()] = {
        .type = CircuitOperations::MacronodeOperationType::BINARY_OPERATION,
        .up_displacement_index = up_displacement_index,
        .left_displacement_index = left_displacement_index,
        .op_indices = {binary_op_index}};
}

/**
 * @brief Add displacement operations to the circuit.
 *
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the operations to.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @param graph_op Graph operation.
 * @param up_mode Up mode of the graph operation.
 * @param left_mode Left mode of the graph operation.
 */
template <std::floating_point Float>
void AddDisplacementOperation(Circuit<Float>& circuit,
                              MacronodeToCircuitOps& macronode_to_circuit_ops,
                              const GraphOperation& graph_op, const std::size_t up_mode,
                              const std::size_t left_mode) {
    const auto up_displacement_index =
        AddDisplacement(circuit, up_mode, CalcUpDisplacementParams<Float>(graph_op));
    const auto left_displacement_index =
        AddDisplacement(circuit, left_mode, CalcLeftDisplacementParams<Float>(graph_op));
    macronode_to_circuit_ops[graph_op.macronode()] = {
        .type = CircuitOperations::MacronodeOperationType::DISPLACEMENT_ONLY_OPERATION,
        .up_displacement_index = up_displacement_index,
        .left_displacement_index = left_displacement_index,
        .op_indices = {}};
}

/**
 * @brief Calculate the squeezing level of measurement operations.
 *
 * @tparam Float The floating point type.
 * @param squeezing_level Squeezing level (in dB) of the x-squeezed modes utilized in the quantum
 * computer.
 * @return Squeezing level of measurement operations in dB.
 */
template <std::floating_point Float>
Float CalcMeasurementSqueezingLevel(const Float squeezing_level) {
    return squeezing_level - 10 * std::log10(Float{2});
}

/**
 * @brief Add measurement operations to the circuit.
 *
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the operations to.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @param graph_op Graph operation.
 * @param up_mode Up mode of the graph operation.
 * @param left_mode Left mode of the graph operation.
 * @param measurement_squeezing_level Squeezing level of measurement operations.
 */
template <std::floating_point Float>
void AddMeasurementOperations(Circuit<Float>& circuit,
                              MacronodeToCircuitOps& macronode_to_circuit_ops,
                              const GraphOperation& graph_op, const std::size_t up_mode,
                              const std::size_t left_mode,
                              const Float measurement_squeezing_level) {
    const auto theta = [](const auto& graph_op) {
        const auto params = CalcCircuitOpParams<Float>(graph_op);
        if (params.size() != 1) { throw std::invalid_argument("Size of 'params' must be 1"); }
        return params[0];
    }(graph_op);

    const auto up_displacement_index =
        AddDisplacement(circuit, up_mode, CalcUpDisplacementParams<Float>(graph_op));
    circuit.template EmplaceOperation<bosim::operation::HomodyneSingleMode<Float>>(
        std::vector{up_mode}, std::vector{theta});
    const auto first_measurement_index = circuit.GetMutOperations().size() - 1;
    circuit.template EmplaceOperation<bosim::operation::util::ReplaceBySqueezedState<Float>>(
        std::vector{up_mode},
        std::vector{measurement_squeezing_level, theta - Float{0.5} * std::numbers::pi_v<Float>});
    const auto first_squeezing_index = circuit.GetMutOperations().size() - 1;

    const auto left_displacement_index =
        AddDisplacement(circuit, left_mode, CalcLeftDisplacementParams<Float>(graph_op));
    circuit.template EmplaceOperation<bosim::operation::HomodyneSingleMode<Float>>(
        std::vector{left_mode}, std::vector{theta});
    const auto second_measurement_index = circuit.GetMutOperations().size() - 1;
    circuit.template EmplaceOperation<bosim::operation::util::ReplaceBySqueezedState<Float>>(
        std::vector{left_mode},
        std::vector{measurement_squeezing_level, theta - Float{0.5} * std::numbers::pi_v<Float>});
    const auto second_squeezing_index = circuit.GetMutOperations().size() - 1;

    macronode_to_circuit_ops[graph_op.macronode()] = {
        .type = CircuitOperations::MacronodeOperationType::MEASUREMENT_OPERATION,
        .up_displacement_index = up_displacement_index,
        .left_displacement_index = left_displacement_index,
        .op_indices = {first_measurement_index, first_squeezing_index, second_measurement_index,
                       second_squeezing_index}};
}

/**
 * @brief Add operations corresponding to the given graph operation to the circuit.
 *
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the operations to.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @param graph_op Graph operation.
 * @param up_mode Up mode of the graph operation.
 * @param left_mode Left mode of the graph operation.
 * @param measurement_squeezing_level Squeezing level of measurement operations.
 */
template <std::floating_point Float>
void AddOperations(Circuit<Float>& circuit, MacronodeToCircuitOps& macronode_to_circuit_ops,
                   const GraphOperation& graph_op, const std::size_t up_mode,
                   const std::size_t left_mode, const Float measurement_squeezing_level) {
    switch (graph_op.type()) {
        case GraphOperation::OPERATION_TYPE_PHASE_ROTATION:
            AddUnaryOperations<bosim::operation::PhaseRotation<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_SHEAR_X_INVARIANT:
            AddUnaryOperations<bosim::operation::ShearXInvariant<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_SHEAR_P_INVARIANT:
            AddUnaryOperations<bosim::operation::ShearPInvariant<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_SQUEEZING:
            AddUnaryOperations<bosim::operation::Squeezing<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_SQUEEZING_45:
            AddUnaryOperations<bosim::operation::Squeezing45<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST:
            AddBinaryOperation<bosim::operation::Manual<Float>>(circuit, macronode_to_circuit_ops,
                                                                graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND:
            AddBinaryOperation<bosim::operation::Manual<Float>>(circuit, macronode_to_circuit_ops,
                                                                graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_CONTROLLED_Z:
            AddBinaryOperation<bosim::operation::ControlledZ<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_BEAM_SPLITTER:
            AddBinaryOperation<bosim::operation::BeamSplitter<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_TWO_MODE_SHEAR:
            AddBinaryOperation<bosim::operation::TwoModeShear<Float>>(
                circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_MANUAL:
            AddBinaryOperation<bosim::operation::Manual<Float>>(circuit, macronode_to_circuit_ops,
                                                                graph_op, up_mode, left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_WIRING:
            AddDisplacementOperation(circuit, macronode_to_circuit_ops, graph_op, up_mode,
                                     left_mode);
            break;
        case GraphOperation::OPERATION_TYPE_INITIALIZATION:
        case GraphOperation::OPERATION_TYPE_MEASUREMENT:
            AddMeasurementOperations(circuit, macronode_to_circuit_ops, graph_op, up_mode,
                                     left_mode, measurement_squeezing_level);
            break;
        default: throw std::runtime_error("Unsupported operation type");
    }
}

/**
 * @brief Calculate the indices of the source operations of a non-linear feedforward.
 *
 * @param nlff Non-linear feedforward.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @return Indices of the source operations.
 */
inline std::vector<std::size_t> CalcFFSourceOps(
    const GraphFF& nlff, const MacronodeToCircuitOps& macronode_to_circuit_ops) {
    const auto& src_operations = macronode_to_circuit_ops.at(nlff.from_macronode());
    if (src_operations.type != CircuitOperations::MacronodeOperationType::MEASUREMENT_OPERATION) {
        throw std::runtime_error("Non-linear feedforward source must be a measurement operation.");
    }
    // nlff.from_bd = 0 => m_b (op_indices[0])
    // nlff.from_bd = 1 => m_d (op_indices[2])
    const auto src_operation_index =
        (nlff.from_bd() == 0 ? src_operations.op_indices[0] : src_operations.op_indices[2]);
    return {src_operation_index};
}

/**
 * @brief Calculate the target parameters of a non-linear feedforward.
 *
 * @param nlff Non-linear feedforward.
 * @param macronode_to_circuit_ops Map from macronode index to circuit operations.
 * @return Indices of the target parameters.
 */
inline std::vector<ParamIdx> CalcFFTargetParams(
    const GraphFF& nlff, const MacronodeToCircuitOps& macronode_to_circuit_ops) {
    const auto& target_operations = macronode_to_circuit_ops.at(nlff.to_macronode());

    auto target_params = std::vector<ParamIdx>{};
    const auto param_index = nlff.to_parameter();

    if (param_index < 4) {
        if (target_operations.type ==
            CircuitOperations::MacronodeOperationType::MEASUREMENT_OPERATION) {
            if (target_operations.op_indices.size() != 4) {
                throw std::logic_error("Measurement operation must have 4 operations\n");
            }
            for (auto i = std::size_t{0}; i < target_operations.op_indices.size(); ++i) {
                target_params.emplace_back(target_operations.op_indices[i], i % 2);
            }
        } else {
            for (const auto op_idx : target_operations.op_indices) {
                target_params.emplace_back(op_idx, param_index);
            }
        }
    } else if (param_index == 4 || param_index == 5) {
        // The target operation is `displacement_k_minus_1`
        const auto target_op_idx = target_operations.up_displacement_index;
        // param_index = 4 -> x: 0
        // param_index = 5 -> p: 1
        const auto displacement_param_index = param_index - 4;
        target_params.emplace_back(target_op_idx, displacement_param_index);
    } else if (param_index == 6 || param_index == 7) {
        // The target operation is `displacement_k_minus_n`
        const auto target_op_idx = target_operations.left_displacement_index;
        // param_index = 6 -> x: 0
        // param_index = 7 -> p: 1
        const auto displacement_param_index = param_index - 6;
        target_params.emplace_back(target_op_idx, displacement_param_index);
    } else {
        throw std::runtime_error("Invalid target parameter of non-linear feedforward.");
    }

    return target_params;
}

/**
 * @brief Construct a non-linear feedforward function.
 *
 * @tparam Float The floating point type.
 * @param ff_func Non-linear feedforward function.
 * @param op_variant Target operation of the non-linear feedforward.
 * @return Non-linear feedforward function.
 */
template <std::floating_point Float>
bosim::FFFunc<Float> ConstructFFFunc(const bosim::FFFunc<Float>& ff_func,
                                     const OperationVariant<Float>& op_variant) {
    if (std::holds_alternative<std::unique_ptr<operation::util::ReplaceBySqueezedState<Float>>>(
            op_variant)) {
        return bosim::FFFunc<Float>{[ff_func](const std::vector<Float>& args) -> Float {
            return static_cast<Float>(ff_func(args) - Float{0.5} * std::numbers::pi_v<Float>);
        }};
    } else {
        return ff_func;
    }
}

/**
 * @brief Construct non-linear feedforward functions to parameters of the Manual operation
 * corresponding to an Arbitrary operation.
 *
 * @tparam Float The floating point type.
 * @param ff_funcs_to_arbitrary Non-linear feedforward functions to the Arbitrary operation.
 * @param target_params_idxs_of_arbitrary Target parameter indices of the non-linear feedforward
 * functions to the Arbitrary operation.
 * @param initial_params_of_arbitrary Initial parameter values of the Arbitrary operation.
 * @param is_arbitrary_first Whether the Arbitrary operation is the first operation.
 * @param is_arbitrary_swap Whether the Arbitrary operation is swapped.
 * @return Non-linear feedforward functions to parameters of the Manual operation.
 */
template <std::floating_point Float>
std::array<bosim::FFFunc<Float>, 4> ConstructFFFuncsToManual(
    const std::array<std::optional<bosim::FFFunc<Float>>, 3>& ff_funcs_to_arbitrary,
    const std::vector<std::size_t>& target_params_idxs_of_arbitrary,
    const std::vector<Float>& initial_params_of_arbitrary, const bool is_arbitrary_first,
    const bool is_arbitrary_swap) {
    constexpr auto NumParamsOfManual = std::size_t{4};
    auto ff_funcs_to_manual = std::array<bosim::FFFunc<Float>, NumParamsOfManual>{};
    for (auto i = std::size_t{0}; i < NumParamsOfManual; ++i) {
        ff_funcs_to_manual[i] = bosim::FFFunc<Float>{
            [initial_params_of_arbitrary, is_arbitrary_first, is_arbitrary_swap,
             target_params_idxs_of_arbitrary, ff_funcs_to_arbitrary,
             i](const std::vector<Float>& args) -> Float {
                const auto params_of_arbitrary = [&initial_params_of_arbitrary,
                                                  &target_params_idxs_of_arbitrary,
                                                  &ff_funcs_to_arbitrary, &args]() {
                    auto params_of_arbitrary = initial_params_of_arbitrary;
                    for (auto j = std::size_t{0}; j < args.size(); ++j) {
                        // The target parameter index of the j-th argument
                        const auto target_arbitrary_param_idx = target_params_idxs_of_arbitrary[j];
                        // If a feedforward function is set, apply it to update the parameter value.
                        if (ff_funcs_to_arbitrary[target_arbitrary_param_idx].has_value()) {
                            params_of_arbitrary[target_arbitrary_param_idx] =
                                ff_funcs_to_arbitrary[target_arbitrary_param_idx].value()(
                                    std::vector{args[j]});
                        }
                    }
                    return params_of_arbitrary;
                }();
                const auto manual_params =
                    is_arbitrary_first
                        ? CalcParamsFromArbitraryFirst(params_of_arbitrary, is_arbitrary_swap)
                        : CalcParamsFromArbitrarySecond(params_of_arbitrary, is_arbitrary_swap);
                return manual_params[i];
            }};
    }
    return ff_funcs_to_manual;
}

/**
 * @brief Add non-linear feedforward functions targeting a Manual operation to the circuit.
 *
 * @tparam Float The floating point type.
 * @param circuit Circuit to add the non-linear feedforward functions to.
 * @param target_op_idx Index of the target arbitrary operation.
 * @param ffs_to_arbitrary Feedforward objects targeting the Arbitrary operation.
 * @param initial_params_of_arbitrary Initial parameter values of the Arbitrary operation.
 * @param is_arbitrary_first Whether the Arbitrary operation is the first operation.
 * @param is_arbitrary_swap Whether the Arbitrary operation is swapped.
 */
template <std::floating_point Float>
void AddFFsTargetingManual(Circuit<Float>& circuit, const std::size_t target_op_idx,
                           const std::vector<bosim::FeedForward<Float>>& ffs_to_arbitrary,
                           const std::vector<Float>& initial_params_of_arbitrary,
                           const bool is_arbitrary_first, const bool is_arbitrary_swap) {
    // No FFs targeting the Arbitrary operation are set.
    if (ffs_to_arbitrary.empty()) { return; }

    // Source operation indices of FFs targeting the Arbitrary operation.
    auto source_ops_idxs = std::vector<std::size_t>{};
    source_ops_idxs.reserve(ffs_to_arbitrary.size());

    // Target parameter indices of FFs targeting the Arbitrary operation.
    auto target_params_idxs_of_arbitrary = std::vector<std::size_t>{};
    target_params_idxs_of_arbitrary.reserve(ffs_to_arbitrary.size());

    for (const auto& ff : ffs_to_arbitrary) {
        source_ops_idxs.emplace_back(ff.source_ops_idxs.back());
        target_params_idxs_of_arbitrary.emplace_back(ff.target_param.param_idx);
    }

    // In the following cases, FFs targeting Manual operation does not exist:
    // - The target Arbitrary operation is ArbitraryFirst and the target parameter is `alpha`
    // - The target Arbitrary operation is ArbitrarySecond and the target parameter is `lambda`
    if ((is_arbitrary_first && target_params_idxs_of_arbitrary == std::vector<std::size_t>{0}) ||
        (!is_arbitrary_first && target_params_idxs_of_arbitrary == std::vector<std::size_t>{2})) {
        return;
    }

    // Construct an array of FF functions targeting the parameters of the Arbitrary operation.
    // If there is no function targeting a parameter, the corresponding element is set to
    // `std::nullopt`.
    const auto ff_funcs_to_arbitrary = [&ffs_to_arbitrary]() {
        constexpr auto NumParamsOfArbitrary = std::size_t{3};
        auto ff_funcs_to_arbitrary =
            std::array<std::optional<bosim::FFFunc<Float>>, NumParamsOfArbitrary>{};
        ff_funcs_to_arbitrary.fill(std::nullopt);
        for (const auto& ff : ffs_to_arbitrary) {
            ff_funcs_to_arbitrary[ff.target_param.param_idx] = ff.func;
        }
        return ff_funcs_to_arbitrary;
    }();

    // Construct an array of FF functions targeting the parameters of the Manual operation.
    const auto ff_funcs_to_manual = ConstructFFFuncsToManual(
        ff_funcs_to_arbitrary, target_params_idxs_of_arbitrary, initial_params_of_arbitrary,
        is_arbitrary_first, is_arbitrary_swap);

    // Add four FFs to the circuit.
    constexpr auto NumParamsOfManual = std::size_t{4};
    for (auto i = std::size_t{0}; i < NumParamsOfManual; ++i) {
        circuit.AddFF(bosim::FeedForward<Float>{source_ops_idxs,
                                                ParamIdx{.op_idx = target_op_idx, .param_idx = i},
                                                ff_funcs_to_manual[i]});
    }
}

/**
 * @brief Create and return a Circuit object based on the given graph representation.
 *
 * @tparam Float The floating point type.
 * @param graph_repr Graph representation.
 * @param squeezing_level Squeezing level (in dB) of the x-squeezed modes utilized in the quantum
 * computer.
 * @return Circuit object corresponding to the given graph representation.
 */
template <std::floating_point Float>
auto GraphCircuit(const GraphRepresentation& graph_repr, const Float squeezing_level)
    -> Circuit<Float> {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::ConvertGraphToCircuit>{};
#endif
    auto circuit = bosim::Circuit<Float>{};

    const auto measurement_squeezing_level = CalcMeasurementSqueezingLevel(squeezing_level);

    // Sort the graph operations by macronode index.
    const auto graph_operations = [&graph_repr]() {
        auto graph_operations = graph_repr.operations();
        std::sort(
            graph_operations.begin(), graph_operations.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.macronode() < rhs.macronode(); });
        return graph_operations;
    }();

    // The first element is an input mode from up.
    // The remaining elements are input modes from left.
    const auto n_local_macronodes = graph_repr.n_local_macronodes();
    auto input_modes = std::vector<std::size_t>(n_local_macronodes + 1);
    std::iota(input_modes.begin(), input_modes.end(), std::size_t{0});

    const auto n_total_macronodes = n_local_macronodes * graph_repr.n_steps();
    const auto n_graph_operations = graph_operations.size();

    // Map from macronode index to circuit operations.
    auto macronode_to_circuit_ops = MacronodeToCircuitOps{};

    // Map from macronode index to graph operation index of an Arbitrary operation.
    auto arbitrary_macronode_to_graph_op_idx = std::unordered_map<std::uint32_t, std::int32_t>{};

    auto graph_op_index = std::int32_t{0};
    for (auto macronode = std::uint32_t{0}; macronode < n_total_macronodes; ++macronode) {
        auto& up_mode = input_modes[0];
        auto& left_mode = input_modes[macronode % n_local_macronodes + 1];
        if (graph_op_index < n_graph_operations &&
            macronode == graph_operations[graph_op_index].macronode()) {
            const auto& graph_op = graph_operations[graph_op_index];
            AddOperations(circuit, macronode_to_circuit_ops, graph_op, up_mode, left_mode,
                          measurement_squeezing_level);
            if (graph_op.swap()) { std::swap(up_mode, left_mode); }
            // If the graph operation is an Arbitrary operation, store the macronode index and graph
            // operation index.
            if (graph_op.type() == GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST ||
                graph_op.type() == GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND) {
                arbitrary_macronode_to_graph_op_idx[macronode] = graph_op_index;
            }
            ++graph_op_index;
        } else {
            // If the macronode is not in the graph representation,
            // assume it is a wiring through operation.
            const auto dummy_graph_op = [](const auto macronode) {
                auto wiring_through = GraphOperation{};
                wiring_through.set_type(GraphOperation::OPERATION_TYPE_WIRING);

                auto zero_displacement = GraphOperation::Displacement{};
                zero_displacement.set_x(0.0f);
                zero_displacement.set_p(0.0f);

                wiring_through.mutable_displacement_k_minus_1()->CopyFrom(zero_displacement);
                wiring_through.mutable_displacement_k_minus_n()->CopyFrom(zero_displacement);

                wiring_through.set_macronode(macronode);
                wiring_through.set_swap(false);
                wiring_through.set_readout(false);
                return wiring_through;
            }(macronode);

            AddOperations(circuit, macronode_to_circuit_ops, dummy_graph_op, up_mode, left_mode,
                          measurement_squeezing_level);
        }
    }

    // Non-linear feedforward functions.
    auto python_functions = std::vector<python::PythonFeedForward>{};
    python_functions.reserve(static_cast<std::size_t>(graph_repr.functions().size()));
    for (const auto& function : graph_repr.functions()) {
        const auto codes = std::vector<std::string>(function.code().begin(), function.code().end());
        python_functions.emplace_back(codes);
    }

    // Map from macronode index of an Arbitrary operation to FFs targeting it.
    auto macronode_to_arbitrary_ffs =
        std::unordered_map<std::uint32_t, std::vector<bosim::FeedForward<Float>>>{};

    // Add non-linear feedforward.
    for (const auto& nlff : graph_repr.nlffs()) {
        const auto source_ops_idxs = CalcFFSourceOps(nlff, macronode_to_circuit_ops);
        const auto target_params = CalcFFTargetParams(nlff, macronode_to_circuit_ops);
        const auto func = bosim::python::ConvertToFFFunc<Float>(python_functions[nlff.function()]);
        for (const auto& target_param : target_params) {
            const auto ff_func =
                ConstructFFFunc(func, circuit.GetMutOperations().at(target_param.op_idx));
            const auto ff = bosim::FeedForward<Float>{source_ops_idxs, target_param, ff_func};
            // If the FF target is a parameter of an arbitrary operation, store it for later.
            if (arbitrary_macronode_to_graph_op_idx.contains(nlff.to_macronode()) &&
                nlff.to_parameter() < 4) {
                macronode_to_arbitrary_ffs[nlff.to_macronode()].emplace_back(ff);
            } else {
                circuit.AddFF(ff);
            }
        }
    }

    // Sort FFs targeting a same Arbitrary operation by the source operation index.
    for (auto& [_, ffs_to_arbitrary] : macronode_to_arbitrary_ffs) {
        std::sort(ffs_to_arbitrary.begin(), ffs_to_arbitrary.end(),
                  [](const auto& ff1, const auto& ff2) {
                      return ff1.source_ops_idxs.back() < ff2.source_ops_idxs.back();
                  });
    }

    // Add FFs targeting Arbitrary operations
    for (const auto& [macronode, ffs_to_arbitrary] : macronode_to_arbitrary_ffs) {
        // Graph / circuit operation index of the Arbitrary operation.
        const auto graph_op_idx = arbitrary_macronode_to_graph_op_idx.at(macronode);
        const auto target_op_idx = [&macronode_to_circuit_ops](const auto macronode) {
            const auto op_indices = macronode_to_circuit_ops.at(macronode).op_indices;
            if (op_indices.size() != 1) {
                throw std::logic_error("Arbitrary operation must have exactly 1 Manual operation");
            }
            return op_indices.back();
        }(macronode);

        const auto& graph_op = graph_operations[graph_op_idx];
        const auto params_of_arbitrary =
            std::vector<Float>(graph_op.parameters().begin(), graph_op.parameters().end());
        const auto is_arbitrary_first =
            graph_op.type() == GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST;
        const auto is_arbitrary_swap = graph_op.swap();

        AddFFsTargetingManual(circuit, target_op_idx, ffs_to_arbitrary, params_of_arbitrary,
                              is_arbitrary_first, is_arbitrary_swap);
    }

    return circuit;
}
}  // namespace bosim::graph
