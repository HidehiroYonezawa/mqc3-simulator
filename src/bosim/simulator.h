#pragma once

#include "bosim/circuit.h"
#include "bosim/result.h"

namespace bosim {
/**
 * @brief A class that executes quantum circuits and stores the result.
 *
 * @tparam Float
 * @details This class provides an interface for executing a quantum circuit and retrieving the
 * computation results.
 */
template <std::floating_point Float>
class Simulator {
public:
    explicit Simulator(const std::size_t n_shots) : n_shots_(n_shots) {
        result_.result.resize(n_shots_);
    }

    void ExecuteOperation(State<Float>& state, const std::size_t op_idx, Circuit<Float>& circuit) {
        const auto shot = state.GetShot();
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::ExecuteOperation>{shot};
#endif
        std::visit(
            [this, op_idx, &state, shot, &circuit](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                using OperationResultType = std::invoke_result_t<OperationType,
                                                                 State<Float>&>;  // void or Float
                LOG_DEBUG("Applying operation: {}", op_variant->ToString());
                if constexpr (std::is_same_v<OperationResultType, void>) {
                    (*op_variant)(state);
                } else {  // measurement
                    static_assert(std::is_same_v<OperationResultType, Float>);
                    GetMutMeasuredVal(shot, op_idx) = (*op_variant)(state);
                    circuit.ApplyAllFF(op_idx, GetMutMeasuredValues(shot), shot);
                }
            },
            circuit.GetMutOperations()[op_idx]);
    }

    void Execute(State<Float>& state, Circuit<Float>& circuit) {
        [[maybe_unused]] const auto shot = state.GetShot();
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::Execute>{shot};
#endif
        const auto num_ops = circuit.GetMutOperations().size();
        for (std::size_t i = 0; i < num_ops; ++i) { ExecuteOperation(state, i, circuit); }
    }

    const Result<Float>& GetResult() const { return result_; }
    Result<Float>& GetMutResult() { return result_; }
    const ShotResult<Float>& GetShotResult(const std::size_t shot) const {
        return result_.result[shot];
    }
    ShotResult<Float>& GetMutShotResult(const std::size_t shot) { return result_.result.at(shot); }
    const std::map<std::size_t, Float>& GetMeasuredValues(const std::size_t shot) const {
        return GetShotResult(shot).measured_values;
    }
    std::map<std::size_t, Float>& GetMutMeasuredValues(const std::size_t shot) {
        return GetMutShotResult(shot).measured_values;
    }
    Float GetMeasuredVal(const std::size_t shot, const std::size_t op_idx) const {
        return GetMeasuredValues(shot).at(op_idx);
    }
    Float& GetMutMeasuredVal(const std::size_t shot, const std::size_t op_idx) {
        return GetMutMeasuredValues(shot)[op_idx];
    }

protected:
    std::size_t n_shots_;

    /**
     * @brief Vector of (operation index vs measured value) map
     */
    Result<Float> result_;
};
}  // namespace bosim
