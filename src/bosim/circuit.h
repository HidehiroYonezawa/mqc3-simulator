#pragma once

#include <boost/functional/hash.hpp>
#include <taskflow/taskflow.hpp>

#include <functional>
#include <memory>
#include <variant>
#include <vector>

#include "bosim/exception.h"
#include "bosim/operation.h"

#ifdef BOSIM_BUILD_BENCHMARK
#include "bosim/base/timer.h"
#endif

namespace bosim {
template <std::floating_point Float>
using OperationVariant = std::variant<
    std::unique_ptr<operation::Displacement<Float>>,
    std::unique_ptr<operation::PhaseRotation<Float>>, std::unique_ptr<operation::Squeezing<Float>>,
    std::unique_ptr<operation::Squeezing45<Float>>,
    std::unique_ptr<operation::ShearXInvariant<Float>>,
    std::unique_ptr<operation::ShearPInvariant<Float>>,
    std::unique_ptr<operation::Arbitrary<Float>>, std::unique_ptr<operation::BeamSplitter<Float>>,
    std::unique_ptr<operation::util::BeamSplitter50<Float>>,
    std::unique_ptr<operation::util::BeamSplitterStd<Float>>,
    std::unique_ptr<operation::ControlledZ<Float>>, std::unique_ptr<operation::TwoModeShear<Float>>,
    std::unique_ptr<operation::Manual<Float>>,
    std::unique_ptr<operation::HomodyneSingleModeX<Float>>,
    std::unique_ptr<operation::HomodyneSingleMode<Float>>,
    std::unique_ptr<operation::util::ReplaceBySqueezedState<Float>>>;

template <std::floating_point Float>
using FFFunc = std::function<Float(const std::vector<Float>&)>;

/**
 * @brief A structure representing an operation index and its parameter index.
 */
struct ParamIdx {
    std::size_t op_idx;
    std::size_t param_idx;
    bool operator==(const ParamIdx&) const = default;
};

/**
 * @brief A class that manages feedforward operations and parameters.
 *
 * @tparam Float
 * @details This class handles multiple source operation indices and a target operation parameter
 * index.
 */
template <std::floating_point Float>
class FeedForward {
public:
    explicit FeedForward(const std::vector<std::size_t>& source_ops_idxs_,
                         const ParamIdx& target_param_, FFFunc<Float> func_)
        : source_ops_idxs(source_ops_idxs_), target_param(target_param_), func(func_) {
        if (std::ranges::adjacent_find(source_ops_idxs.begin(), source_ops_idxs.end()) !=
            source_ops_idxs.end()) {
            throw SimulationError(error::InvalidFF,
                                  "A duplicate index was found in the source of a feedforward. "
                                  "Indices must be unique.");
        }
    }

    std::vector<std::size_t> source_ops_idxs;
    ParamIdx target_param;
    FFFunc<Float> func;

#ifndef NDEBUG
    std::string ToString() const {
        std::string str = "FeedForward{";
        str += "source_ops_idxs: [";
        for (const auto& idx : source_ops_idxs) { str += std::to_string(idx) + ", "; }
        str += "], ";
        str += "target_param: {" + std::to_string(target_param.op_idx) + ", " +
               std::to_string(target_param.param_idx) + "}}";
        return str;
    }
#endif
};

/**
 * @brief A class representing a quantum circuit.
 *
 * @tparam Float
 * @details This class provides an interface for constructing and managing a quantum circuit,
 * including adding operations, handling feedforward settings, and retrieving circuit information.
 */
template <std::floating_point Float>
class Circuit {
public:
    Circuit() = default;
    Circuit(const Circuit&) = delete;
    Circuit(Circuit&&) = default;
    Circuit& operator=(const Circuit&) = delete;
    Circuit& operator=(Circuit&&) = default;

    template <typename Op>
    void EmplaceOperation(const std::vector<std::size_t>& modes, const std::vector<Float>& params) {
        operations_.emplace_back(std::make_unique<Op>(modes, params));
    }

    /**
     * @brief Add operation
     *
     * @tparam Op Operation type
     * @tparam Args Argument types of operation constructor
     * @param args Arguments of operation constructor
     */
    template <typename Op, typename... Args>
    void EmplaceOperation(Args&&... args) {
        operations_.emplace_back(std::make_unique<Op>(std::forward<Args>(args)...));
    }

    Circuit<Float> Copy() const {
        auto ret = Circuit<Float>{};
        for (const auto& op : operations_) {
            std::visit(
                [&ret](auto&& op_variant) {
                    using OperationType = std::decay_t<decltype(*op_variant)>;
                    ret.template EmplaceOperation<OperationType>((*op_variant).Modes(),
                                                                 (*op_variant).Params());
                },
                op);
        }
        for (const auto& ff : ff_instance_list_) { ret.AddFF(ff); }
        return ret;
    }

    void AddFF(const FeedForward<Float>& ff) {
        if (ff_target_params_.contains(ff.target_param)) {
            throw SimulationError(error::InvalidFF,
                                  "Duplicate feedforward target parameter detected. Each "
                                  "feedforward must have a unique target.");
        }
        ff_instance_list_.emplace_back(ff);
        ff_funcs_.emplace(std::ranges::max(ff.source_ops_idxs), &ff_instance_list_.back());
        ff_target_params_[ff.target_param] = &ff_instance_list_.back();
    }

    /**
     * @brief Executes feedforward based on measurement results.
     *
     * @param ff Feedforward function.
     * @param measured_values Map associating operation indices with measured values.
     * @details This method applies the feedforward function to modify the circuit
     * dynamically according to the provided measured values.
     */
    void ApplyFF(const FeedForward<Float>& ff,
                 const std::map<std::size_t, Float>& measured_values) {
        const std::vector<std::size_t>& source_ops_idxs = ff.source_ops_idxs;
        auto measured_vals = std::vector<Float>{};
        for (const auto& idx : source_ops_idxs) {
            measured_vals.push_back(measured_values.at(idx));
        }
        std::visit(
            [&ff, &measured_vals](auto&& tar_op_variant) {
                (*tar_op_variant).GetMutParam(ff.target_param.param_idx) = ff.func(measured_vals);
            },
            operations_[ff.target_param.op_idx]);
    }

    /**
     * @brief Executes all feedforward functions where the last source operation matches the given
     * index.
     *
     * @param src_op_idx Source operation index.
     * @param measured_values Map associating operation indices with measured values.
     * @param shot Shot index used for measuring execution time.
     * @details This method finds all feedforward operations whose final source operation
     * corresponds to the specified index and executes them.
     */
    void ApplyAllFF(const std::size_t src_op_idx,
                    const std::map<std::size_t, Float>& measured_values,
                    [[maybe_unused]] const std::size_t shot) {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::ApplyAllFF>{shot};
#endif
        auto range = ff_funcs_.equal_range(src_op_idx);
        // for each FF function that has an operation 'src_op_idx' as the largest-index source
        // operation
        for (auto it = range.first; it != range.second; ++it) {
            const FeedForward<Float>& ff = *it->second;
            ApplyFF(ff, measured_values);
        }
    }

private:
    struct ParamIdxHash {
        std::size_t operator()(ParamIdx const& idx) const noexcept {
            std::size_t seed = 0;
            boost::hash_combine(seed, idx.op_idx);
            boost::hash_combine(seed, idx.param_idx);
            return seed;
        }
    };

public:
    const std::vector<OperationVariant<Float>>& GetOperations() const { return operations_; }
    std::vector<OperationVariant<Float>>& GetMutOperations() { return operations_; }
    const std::unordered_multimap<std::size_t, const FeedForward<Float>*>& GetFFFuncs() const {
        return ff_funcs_;
    }
    const auto& GetFFTargetParams() const { return ff_target_params_; }

    /**
     * @brief Get reference to unique pointer to operation specified by index
     *
     * @tparam Op Operation type
     * @param op_idx Operation index
     * @return std::unique_ptr<Op>& Reference to unique pointer to operation
     */
    template <typename Op>
    std::unique_ptr<Op>& GetOpPtr(std::size_t op_idx) {
        return std::get<std::unique_ptr<Op>>(operations_[op_idx]);
    }
    template <typename Op>
    const std::unique_ptr<Op>& GetOpPtr(std::size_t op_idx) const {
        return std::get<std::unique_ptr<Op>>(operations_[op_idx]);
    }

    void ReserveFFFuncs(const std::size_t size) { ff_funcs_.reserve(size); }
    void ReserveFFTargetParams(const std::size_t size) { ff_target_params_.reserve(size); }

    // NOLINTBEGIN(readability-identifier-naming)
    [[nodiscard]] auto begin() { return operations_.begin(); }
    [[nodiscard]] auto begin() const { return operations_.begin(); }
    [[nodiscard]] auto end() { return operations_.end(); }
    [[nodiscard]] auto end() const { return operations_.end(); }
    // NOLINTEND(readability-identifier-naming)

#ifndef NDEBUG
    std::string ToString() const {
        auto str = std::string{"Operations:\n"};
        for (auto op_idx = std::size_t{0}; const auto& op : operations_) {
            std::visit(
                [op_idx, &str](auto&& op_variant) {
                    str += fmt::format("{}: {}\n", op_idx, op_variant->ToString());
                },
                op);
            ++op_idx;
        }
        str += "Feedforward functions:\n";
        for (const auto& [src_op_idx, ff] : ff_funcs_) {
            str += fmt::format("{}: {}\n", src_op_idx, ff->ToString());
        }
        return str;
    }
#endif

private:
    std::vector<OperationVariant<Float>> operations_;

    std::list<FeedForward<Float>> ff_instance_list_;

    /**
     * @brief The largest index of source operations vs feedforward info
     */
    std::unordered_multimap<std::size_t, const FeedForward<Float>*> ff_funcs_;

    /**
     * @brief Target parameter vs feedforward info
     */
    std::unordered_map<ParamIdx, const FeedForward<Float>*, ParamIdxHash> ff_target_params_;
};

}  // namespace bosim
