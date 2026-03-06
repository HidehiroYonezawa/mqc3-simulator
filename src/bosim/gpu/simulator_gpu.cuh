#pragma once

#include "bosim/gpu/state_gpu.cuh"
#include "bosim/simulator.h"

namespace bosim::gpu {
/**
 * @brief A class that executes quantum circuits and stores the result on the GPU.
 *
 * @tparam Float
 * @details This class provides an interface for executing a quantum circuit and retrieving the
 * computation results on the GPU. This class is derived from 'Simulator<Float>'.
 */
template <std::floating_point Float>
class SimulatorGPU : protected Simulator<Float> {
public:
    explicit SimulatorGPU(const std::size_t n_shots) : Simulator<Float>(n_shots) {}

    void ExecuteOperation(StateGPU<Float>& state, const std::size_t op_idx, Circuit<Float>& circuit,
                          cudaStream_t stream) {
        const auto shot = state.GetShot();
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::ExecuteOperation>{shot};
#endif

        std::visit(
            [this, op_idx, &state, shot, &circuit, stream](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                using OperationResultType = std::invoke_result_t<OperationType,
                                                                 State<Float>&>;  // void or Float
                LOG_DEBUG("Applying operation: {}", op_variant->ToString());
                if constexpr (std::is_same_v<
                                  OperationType,
                                  operation::HomodyneSingleModeX<Float>>) {  // measurement
                    const auto mode = op_variant->Modes()[0];
                    const auto selection = op_variant->GetSelection();
                    this->GetMutMeasuredVal(shot, op_idx) =
                        selection ? state.HomodyneSingleModeXGPU(mode, selection.value(), stream)
                                  : state.HomodyneSingleModeXGPU(mode, stream);
                    circuit.ApplyAllFF(op_idx, this->GetMutMeasuredValues(shot), shot);
                } else if constexpr (std::is_same_v<OperationType,
                                                    operation::HomodyneSingleMode<Float>>) {
                    const auto mode = op_variant->Modes()[0];
                    const auto theta = op_variant->GetTheta();
                    const auto selection = op_variant->GetSelection();
                    this->GetMutMeasuredVal(shot, op_idx) =
                        selection
                            ? state.HomodyneSingleModeGPU(mode, theta, selection.value(), stream)
                            : state.HomodyneSingleModeGPU(mode, theta, stream);
                    circuit.ApplyAllFF(op_idx, this->GetMutMeasuredValues(shot), shot);
                } else if constexpr (std::is_same_v<OperationType,
                                                    operation::Displacement<Float>>) {
                    static_assert(std::is_same_v<OperationResultType, void>);

                    const auto mode = op_variant->Modes()[0];
                    const auto x = op_variant->Params()[0];
                    const auto p = op_variant->Params()[1];
                    state.DisplacementGPU(mode, x, p, stream);
                } else if constexpr (std::is_same_v<
                                         OperationType,
                                         operation::util::ReplaceBySqueezedState<Float>>) {
                    static_assert(std::is_same_v<OperationResultType, void>);

                    const auto mode = op_variant->Modes()[0];
                    const auto squeezing_level = op_variant->Params()[0];
                    const auto phi = op_variant->Params()[1];
                    state.ReplaceBySqueezedStateGPU(mode, squeezing_level, phi, stream);
                } else {
                    static_assert(std::is_same_v<OperationResultType, void>);

                    auto operated_indices = op_variant->Modes();
                    for (auto mode : op_variant->Modes()) {
                        operated_indices.push_back(mode + state.NumModes());
                    }
                    state.UpdateStateGPU(operated_indices, op_variant->GetOperationMatrix(),
                                         stream);
                }
            },
            circuit.GetMutOperations()[op_idx]);
    }

    void Execute(StateGPU<Float>& state, Circuit<Float>& circuit, cudaStream_t stream) {
        const auto shot = state.GetShot();
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::Execute>{shot};
#endif

        const auto num_ops = circuit.GetMutOperations().size();
        for (std::size_t i = 0; i < num_ops; ++i) { ExecuteOperation(state, i, circuit, stream); }
    }

    using Simulator<Float>::GetResult;
    using Simulator<Float>::GetMutResult;
    using Simulator<Float>::GetMutShotResult;
    using Simulator<Float>::GetMeasuredVal;

protected:
    using Simulator<Float>::n_shots_;
    using Simulator<Float>::result_;
};

}  // namespace bosim::gpu
