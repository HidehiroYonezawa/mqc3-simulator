#pragma once

#include "bosim/simulate/circuit_repr.h"

#include <gtest/gtest.h>

template <std::floating_point Float>
void AddOperationToPbCircuitRepr(bosim::circuit::PbCircuitRepresentation& pb_circuit_repr,
                                 const bosim::circuit::PbCircuitOperation::OperationType op_type,
                                 const std::vector<std::uint32_t>& modes,
                                 const std::vector<Float>& params) {
    auto* op = pb_circuit_repr.add_operations();
    op->set_type(op_type);
    for (const auto mode : modes) { op->add_modes(mode); }
    for (const auto param : params) { op->add_parameters(static_cast<float>(param)); }
}

inline void AddFFToPbCircuitRepr(bosim::circuit::PbCircuitRepresentation& pb_circuit_repr,
                                 const std::uint32_t function_index,
                                 const std::uint32_t from_operation,
                                 const std::uint32_t to_operation,
                                 const std::uint32_t to_parameter) {
    auto* nlff = pb_circuit_repr.add_nlffs();
    nlff->set_function(function_index);
    nlff->set_from_operation(from_operation);
    nlff->set_to_operation(to_operation);
    nlff->set_to_parameter(to_parameter);
}

template <typename ExpectedOpType, std::floating_point Float>
void CheckOperation(const bosim::OperationVariant<Float>& op_variant,
                    const std::vector<std::size_t>& expected_modes,
                    const std::vector<Float>& expected_params) {
    std::visit(
        [&expected_modes, &expected_params](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<ExpectedOpType, OperationType>) {
                EXPECT_EQ(expected_modes, op_variant->Modes());
                EXPECT_EQ(expected_params, op_variant->Params());
            } else {
                FAIL();
            }
        },
        op_variant);
}

template <std::floating_point Float>
void CheckFeedForwardParams(
    const std::unordered_multimap<std::size_t, const bosim::FeedForward<Float>*>& nlffs,
    const std::size_t largest_src_op_index, const std::size_t expected_n_ffs,
    const std::vector<bosim::ParamIdx>& expected_target_params) {
    // Source operation
    EXPECT_EQ(expected_n_ffs, nlffs.count(largest_src_op_index));
    auto range = nlffs.equal_range(largest_src_op_index);
    for (auto it = range.first; it != range.second; ++it) {
        EXPECT_EQ(largest_src_op_index, it->second->source_ops_idxs.back());
    }

    // Target parameters
    std::vector<bosim::ParamIdx> target_params;
    std::transform(range.first, range.second, std::back_inserter(target_params),
                   [](const auto& p) { return p.second->target_param; });
    std::sort(target_params.begin(), target_params.end(), [](const auto& a, const auto& b) {
        if (a.op_idx == b.op_idx) { return a.param_idx < b.param_idx; }
        return a.op_idx < b.op_idx;
    });
    EXPECT_EQ(expected_target_params, target_params);
}

template <std::floating_point Float>
void CheckFeedForwardFunction(
    const std::vector<std::pair<std::vector<float>, float>>& expected_func_mappings,
    const bosim::FFFunc<Float>& ff_func) {
    for (const auto& [args, expected_output] : expected_func_mappings) {
        const auto actual_output = ff_func(args);
        EXPECT_EQ(expected_output, actual_output);
    }
}
