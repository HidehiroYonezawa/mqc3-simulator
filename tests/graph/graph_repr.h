#pragma once

#include "bosim/simulate/graph_repr.h"

template <std::floating_point Float>
void AddOperationToGraphRepr(
    bosim::graph::GraphRepresentation& graph_repr, const std::uint32_t macronode,
    const bool is_swap, const std::vector<Float>& params,
    const bosim::graph::GraphOperation::OperationType op_type,
    const std::optional<mqc3_cloud::program::v1::GraphOperation::Displacement>&
        displacement_k_minus_1,
    const std::optional<mqc3_cloud::program::v1::GraphOperation::Displacement>&
        displacement_k_minus_n,
    const bool is_readout = false) {
    auto* op = graph_repr.add_operations();
    op->set_type(op_type);
    if (displacement_k_minus_1.has_value()) {
        op->mutable_displacement_k_minus_1()->CopyFrom(displacement_k_minus_1.value());
    }
    if (displacement_k_minus_n.has_value()) {
        op->mutable_displacement_k_minus_n()->CopyFrom(displacement_k_minus_n.value());
    }
    op->set_macronode(macronode);
    op->set_swap(is_swap);
    for (const auto param : params) { op->add_parameters(static_cast<float>(param)); }
    op->set_readout(is_readout);
}

inline void AddFFToGraphRepr(bosim::graph::GraphRepresentation& graph_repr,
                             const std::uint32_t src_macronode, const std::uint32_t src_micronode,
                             const std::uint32_t target_macronode, const std::uint32_t target_param,
                             const std::uint32_t function_index) {
    auto* nlff = graph_repr.add_nlffs();
    nlff->set_from_macronode(src_macronode);
    nlff->set_from_bd(src_micronode);
    nlff->set_to_macronode(target_macronode);
    nlff->set_function(function_index);
    nlff->set_to_parameter(target_param);
}
