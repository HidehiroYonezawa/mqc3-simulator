#pragma once

#include "bosim/circuit.h"
#include "bosim/python/function.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"

namespace bosim::circuit {

using PbBosonicState = mqc3_cloud::program::v1::BosonicState;
using PbCircuitRepresentation = mqc3_cloud::program::v1::CircuitRepresentation;
using PbCircuitOperation = mqc3_cloud::program::v1::CircuitOperation;
using PbShotMeasuredValue = mqc3_cloud::program::v1::CircuitResult_ShotMeasuredValue;

/**
 * @brief Converts a protobuf complex number to a std::complex.
 *
 * @tparam Float
 * @param complex The protobuf complex number.
 * @return std::complex<Float> The std::complex.
 */
template <std::floating_point Float>
std::complex<Float> GetComplex(const mqc3_cloud::common::v1::Complex& complex) {
    return {complex.real(), complex.imag()};
}

/**
 * @brief Converts a protobuf bosonic state to a bosim::SingleModeMultiPeak.
 *
 * @tparam Float
 * @param initial_state The protobuf bosonic state.
 * @return bosim::SingleModeMultiPeak<Float> The bosim::SingleModeMultiPeak.
 */
template <std::floating_point Float>
inline bosim::SingleModeMultiPeak<Float> GetInitialState(const PbBosonicState* initial_state) {
    using namespace bosim;
    auto state = SingleModeMultiPeak<Float>();
    for (auto i = 0; i < initial_state->gaussian_states_size(); ++i) {
        const auto& gaussian_state = initial_state->gaussian_states(i);
        Vector2C<Float> mean;
        mean << GetComplex<Float>(gaussian_state.mean(0)),
            GetComplex<Float>(gaussian_state.mean(1));
        Matrix2D<Float> cov;
        cov << gaussian_state.cov(0), gaussian_state.cov(1), gaussian_state.cov(2),
            gaussian_state.cov(3);
        state.peaks.emplace_back(GetComplex<Float>(initial_state->coeffs(i)), mean, cov);
    }
    return state;
}

/**
 * @brief Get the initial state object from a protobuf circuit representation.
 *
 * @tparam Float
 * @param circuit_repr The protobuf circuit representation.
 * @return bosim::State<Float> The bosim::State.
 */
template <std::floating_point Float>
inline bosim::State<Float> InitializeState(const PbCircuitRepresentation* circuit_repr) {
    using namespace bosim;
    auto modes = std::vector<SingleModeMultiPeak<Float>>();
    for (auto i = 0; i < circuit_repr->initial_states_size(); ++i) {
        const auto& initial_state = circuit_repr->initial_states(i);
        modes.emplace_back(GetInitialState<Float>(&initial_state.bosonic()));
    }
    return State<Float>(modes);
}

/**
 * @brief Convert a protobuf circuit representation to a bosim circuit.
 *
 * @tparam Float
 * @param pb CircuitRepresentation object in protobuf format.
 * @return Circuit<Float> bosim circuit.
 */
template <std::floating_point Float>
auto CircuitFromProto(const PbCircuitRepresentation* pb) -> Circuit<Float> {
    using namespace bosim;
    using namespace mqc3_cloud::program::v1;
    auto circuit = Circuit<Float>();
    // Operations
    for (const auto& operation : pb->operations()) {
        switch (operation.type()) {
            case CircuitOperation_OperationType_OPERATION_TYPE_MEASUREMENT:
                circuit.template EmplaceOperation<bosim::operation::HomodyneSingleMode<Float>>(
                    operation.modes(0), operation.parameters(0));
                break;
            case CircuitOperation_OperationType_OPERATION_TYPE_DISPLACEMENT:
                circuit.template EmplaceOperation<bosim::operation::Displacement<Float>>(
                    operation.modes(0), operation.parameters(0), operation.parameters(1));
                break;
            case CircuitOperation_OperationType_OPERATION_TYPE_PHASE_ROTATION: {
                circuit.template EmplaceOperation<bosim::operation::PhaseRotation<Float>>(
                    operation.modes(0), operation.parameters(0));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_SHEAR_X_INVARIANT: {
                circuit.template EmplaceOperation<bosim::operation::ShearXInvariant<Float>>(
                    operation.modes(0), operation.parameters(0));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_SHEAR_P_INVARIANT: {
                circuit.template EmplaceOperation<bosim::operation::ShearPInvariant<Float>>(
                    operation.modes(0), operation.parameters(0));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_SQUEEZING: {
                circuit.template EmplaceOperation<bosim::operation::Squeezing<Float>>(
                    operation.modes(0), operation.parameters(0));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_SQUEEZING_45: {
                circuit.template EmplaceOperation<bosim::operation::Squeezing45<Float>>(
                    operation.modes(0), operation.parameters(0));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_ARBITRARY: {
                circuit.template EmplaceOperation<bosim::operation::Arbitrary<Float>>(
                    operation.modes(0), operation.parameters(0), operation.parameters(1),
                    operation.parameters(2));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_CONTROLLED_Z: {
                circuit.template EmplaceOperation<bosim::operation::ControlledZ<Float>>(
                    operation.modes(0), operation.modes(1), operation.parameters(0));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_BEAM_SPLITTER: {
                circuit.template EmplaceOperation<bosim::operation::BeamSplitter<Float>>(
                    operation.modes(0), operation.modes(1), operation.parameters(0),
                    operation.parameters(1));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_TWO_MODE_SHEAR: {
                circuit.template EmplaceOperation<bosim::operation::TwoModeShear<Float>>(
                    operation.modes(0), operation.modes(1), operation.parameters(0),
                    operation.parameters(1));
                break;
            }
            case CircuitOperation_OperationType_OPERATION_TYPE_MANUAL: {
                circuit.template EmplaceOperation<bosim::operation::Manual<Float>>(
                    operation.modes(0), operation.modes(1), operation.parameters(0),
                    operation.parameters(1), operation.parameters(2), operation.parameters(3));
                break;
            }
            default:
                throw std::runtime_error(
                    fmt::format("Invalid operation type: {}",
                                CircuitOperation_OperationType_Name(operation.type())));
        }
    }

    // Feedforward function indices.
    auto py_func_idxs = std::vector<python::PythonFeedForward>{};
    py_func_idxs.reserve(static_cast<std::size_t>(pb->functions().size()));
    for (const auto& function : pb->functions()) {
        const auto codes = std::vector<std::string>(function.code().begin(), function.code().end());
        py_func_idxs.emplace_back(codes);
    }

    // Feedforwards
    for (const auto& nlff : pb->nlffs()) {
        const std::vector<std::size_t> source_ops_idxs{nlff.from_operation()};
        const bosim::ParamIdx param_idx{nlff.to_operation(), nlff.to_parameter()};
        const auto func = bosim::python::ConvertToFFFunc<Float>(py_func_idxs[nlff.function()]);
        auto ff = bosim::FeedForward<Float>{source_ops_idxs, param_idx, func};
        circuit.AddFF(ff);
    }

    return circuit;
}

/**
 * @brief Convert measured values in a shot to protobuf message.
 *
 * @tparam Float
 * @param circuit bosim::Circuit.
 * @param measured_values Map of operation index vs measured value in a shot.
 * @param out Protobuf message.
 */
template <std::floating_point Float>
void SetMeasuredValues(const bosim::Circuit<Float>& circuit,
                       const std::map<std::size_t, Float>& measured_values,
                       PbShotMeasuredValue* out) {
    out->mutable_measured_vals()->Reserve(static_cast<std::int32_t>(measured_values.size()));
    for (const auto& [operation_index, value] : measured_values) {
        auto* tmp = out->add_measured_vals();
        const auto& op =
            circuit.template GetOpPtr<bosim::operation::HomodyneSingleMode<Float>>(operation_index);
        // Set the index of the measured mode.
        tmp->set_index(static_cast<std::uint32_t>(op->Modes()[0]));
        tmp->set_value(static_cast<float>(value));
    }
}

/**
 * @brief Convert final state in a shot to protobuf message.
 *
 * @tparam Float
 * @param state The final state.
 * @param out Protobuf message.
 */
template <std::floating_point Float>
void SetState(const bosim::State<Float>& state, PbBosonicState* out) {
    out->mutable_gaussian_states()->Reserve(static_cast<std::int32_t>(state.NumPeaks()));
    out->mutable_coeffs()->Reserve(static_cast<std::int32_t>(state.NumPeaks()));
    for (const auto& peak : state.GetPeaks()) {
        const auto& coeff = peak.GetCoeff();
        auto* tmp_peak = out->add_gaussian_states();
        auto* tmp_coeffs = out->add_coeffs();
        tmp_coeffs->set_real(static_cast<float>(coeff.real()));
        tmp_coeffs->set_imag(static_cast<float>(coeff.imag()));
        tmp_peak->mutable_mean()->Reserve(static_cast<std::int32_t>(peak.GetMean().size()));
        for (const auto& mean : peak.GetMean()) {
            auto* tmp_mean = tmp_peak->add_mean();
            tmp_mean->set_real(static_cast<float>(mean.real()));
            tmp_mean->set_imag(static_cast<float>(mean.imag()));
        }
        tmp_peak->mutable_cov()->Reserve(static_cast<std::int32_t>(peak.GetCov().size()));
        for (auto i = 0; i < peak.GetCov().rows(); ++i) {
            for (const auto& cov : peak.GetCov().row(i)) {
                tmp_peak->add_cov(static_cast<float>(cov));
            }
        }
    }
}
}  // namespace bosim::circuit
