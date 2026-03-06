#pragma once

#include "bosim/result.h"
#include "bosim/simulate/graph_repr.h"

namespace bosim::graph {

using mqc3_cloud::program::v1::GraphRepresentation;
using mqc3_cloud::program::v1::GraphResult;

/**
 * @brief Information about a measurement macronode.
 */
struct MeasurementMacronodeInfo {
    /**
     * @brief Index of the measurement macronode.
     */
    std::uint32_t macronode;
    /**
     * @brief Whether the up mode is measured.
     */
    bool is_up_mode_measured;
};

/**
 * @brief Map from measurement operation index to macronode information.
 */
using MeasurementOpIdxToMacronodeInfo = std::unordered_map<std::size_t, MeasurementMacronodeInfo>;

/**
 * @brief Update the map from measurement operation index to macronode information.
 *
 * @param graph_op Graph operation.
 * @param first_op_idx First index of the measurement operations corresponding to the graph
 * operation.
 * @param measurement_op_idx_to_macronode_info Map from measurement operation index to macronode
 * information.
 */
inline void UpdateMeasurementOpMap(
    const GraphOperation& graph_op, const std::size_t first_op_idx,
    MeasurementOpIdxToMacronodeInfo& measurement_op_idx_to_macronode_info) {
    const auto macronode = graph_op.macronode();
    const auto up_measurement_op_idx = first_op_idx + 1;
    const auto left_measurement_op_idx = first_op_idx + 4;

    if (measurement_op_idx_to_macronode_info.contains(up_measurement_op_idx)) {
        throw std::runtime_error("Measurement operation index has already been added to the map.");
    }
    measurement_op_idx_to_macronode_info[up_measurement_op_idx] = {.macronode = macronode,
                                                                   .is_up_mode_measured = true};

    if (measurement_op_idx_to_macronode_info.contains(left_measurement_op_idx)) {
        throw std::runtime_error("Measurement operation index has already been added to the map.");
    }
    measurement_op_idx_to_macronode_info[left_measurement_op_idx] = {.macronode = macronode,
                                                                     .is_up_mode_measured = false};
}

/**
 * @brief Count the number of operations corresponding to the graph operation.
 *
 * @param graph_op Graph operation.
 * @return Number of operations corresponding to the graph operation.
 */
inline std::size_t CountOperations(const GraphOperation& graph_op) {
    switch (graph_op.type()) {
        case GraphOperation::OPERATION_TYPE_PHASE_ROTATION:
        case GraphOperation::OPERATION_TYPE_SHEAR_X_INVARIANT:
        case GraphOperation::OPERATION_TYPE_SHEAR_P_INVARIANT:
        case GraphOperation::OPERATION_TYPE_SQUEEZING:
        case GraphOperation::OPERATION_TYPE_SQUEEZING_45: return 4;
        case GraphOperation::OPERATION_TYPE_ARBITRARY_FIRST:
        case GraphOperation::OPERATION_TYPE_ARBITRARY_SECOND:
        case GraphOperation::OPERATION_TYPE_CONTROLLED_Z:
        case GraphOperation::OPERATION_TYPE_BEAM_SPLITTER:
        case GraphOperation::OPERATION_TYPE_TWO_MODE_SHEAR:
        case GraphOperation::OPERATION_TYPE_MANUAL: return 3;
        case GraphOperation::OPERATION_TYPE_WIRING: return 2;
        case GraphOperation::OPERATION_TYPE_INITIALIZATION:
        case GraphOperation::OPERATION_TYPE_MEASUREMENT: return 6;
        default: throw std::runtime_error("Unsupported operation type");
    }
}

/**
 * @brief Construct a map from measurement operation index to macronode information.
 *
 * @param graph_repr Graph representation.
 * @return Map from measurement operation index to macronode information.
 * @note Only the macronodes with readout = True are added to the map.
 */
inline auto ConstructMeasurementOpMap(const GraphRepresentation& graph_repr) {
    auto measurement_op_idx_to_macronode_info = MeasurementOpIdxToMacronodeInfo{};

    // Sort the graph operations by macronode index.
    const auto graph_operations = [&graph_repr]() {
        auto graph_operations = graph_repr.operations();
        std::sort(
            graph_operations.begin(), graph_operations.end(),
            [](const auto& lhs, const auto& rhs) { return lhs.macronode() < rhs.macronode(); });
        return graph_operations;
    }();

    const auto n_local_macronodes = graph_repr.n_local_macronodes();
    const auto n_total_macronodes = n_local_macronodes * graph_repr.n_steps();
    const auto n_graph_operations = graph_operations.size();

    auto graph_op_index = std::int32_t{0};
    auto circuit_op_index = std::size_t{0};
    for (auto macronode = std::uint32_t{0}; macronode < n_total_macronodes; ++macronode) {
        if (graph_op_index < n_graph_operations &&
            macronode == graph_operations[graph_op_index].macronode()) {
            const auto& graph_op = graph_operations[graph_op_index];
            if ((graph_op.type() == GraphOperation::OPERATION_TYPE_MEASUREMENT ||
                 graph_op.type() == GraphOperation::OPERATION_TYPE_INITIALIZATION) &&
                graph_op.readout()) {
                UpdateMeasurementOpMap(graph_op, circuit_op_index,
                                       measurement_op_idx_to_macronode_info);
            }
            circuit_op_index += CountOperations(graph_op);
            ++graph_op_index;
        } else {
            // Add two displacement operations for empty macronodes
            circuit_op_index += 2;
        }
    }

    return measurement_op_idx_to_macronode_info;
}

/**
 * @brief Create and return a GraphResult based on the given simulation result and graph
 * representation.
 *
 * @tparam Float The floating point type.
 * @param result Result of a simulation.
 * @param graph_repr Graph representation.
 * @return GraphResult corresponding to the given simulation result.
 */
template <std::floating_point Float>
GraphResult ExtractGraphResult(const Result<Float>& result, const GraphRepresentation& graph_repr) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::ExtractGraphResult>{};
#endif
    const auto measurement_op_idx_to_macronode_info = ConstructMeasurementOpMap(graph_repr);

    auto pb_graph_result = GraphResult{};
    pb_graph_result.mutable_measured_vals()->Reserve(
        static_cast<std::int32_t>(result.result.size()));

    pb_graph_result.set_n_local_macronodes(graph_repr.n_local_macronodes());
    for (const auto& shot_result : result.result) {
        auto macronode_measured_vals =
            std::unordered_map<std::uint32_t, GraphResult::MacronodeMeasuredValue>{};
        for (const auto& [op_idx, measured_val] : shot_result.measured_values) {
            if (measurement_op_idx_to_macronode_info.contains(op_idx)) {
                const auto& [macronode, is_up_mode_measured] =
                    measurement_op_idx_to_macronode_info.at(op_idx);

                macronode_measured_vals[macronode].set_index(macronode);
                if (is_up_mode_measured) {
                    macronode_measured_vals[macronode].set_m_b(static_cast<float>(measured_val));
                } else {
                    macronode_measured_vals[macronode].set_m_d(static_cast<float>(measured_val));
                }
            }
        }

        auto pb_shot_measured_val = GraphResult::ShotMeasuredValue{};
        pb_shot_measured_val.mutable_measured_vals()->Reserve(
            static_cast<std::int32_t>(macronode_measured_vals.size()));
        for (auto& [_, pb_macronode_measured_val] : macronode_measured_vals) {
            pb_shot_measured_val.mutable_measured_vals()->Add(std::move(pb_macronode_measured_val));
        }

        pb_graph_result.mutable_measured_vals()->Add(std::move(pb_shot_measured_val));
    }

    return pb_graph_result;
}

}  // namespace bosim::graph
