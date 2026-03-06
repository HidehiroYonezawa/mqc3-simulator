#pragma once

#include "bosim/circuit.h"

#include "bosim/python/function.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"

template <std::floating_point Float>
bosim::Circuit<Float> InitializeCircuit(
    const mqc3_cloud::program::v1::CircuitRepresentation* circuit_repr) {
    using namespace bosim;
    using namespace mqc3_cloud::program::v1;
    auto circuit = Circuit<Float>();
    // Operations
    for (const auto& operation : circuit_repr->operations()) {
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

    // Feed forwards
    for (const auto& nlff : circuit_repr->nlffs()) {
        const auto func_idx = static_cast<std::int32_t>(nlff.function());
        const std::vector<std::string> codes{circuit_repr->functions(func_idx).code().begin(),
                                             circuit_repr->functions(func_idx).code().end()};

        const std::vector<std::size_t> source_ops_idxs{nlff.from_operation()};
        const bosim::ParamIdx param_idx{nlff.to_operation(), nlff.to_parameter()};

        const bosim::FeedForward<Float> ff(source_ops_idxs, param_idx,
                                           bosim::python::PythonFeedForward(codes));
        circuit.AddFF(ff);
    }

    return circuit;
}

template <std::floating_point Float>
std::complex<Float> GetComplex(const mqc3_cloud::common::v1::Complex& complex) {
    return {complex.real(), complex.imag()};
}

template <std::floating_point Float>
bosim::SingleModeMultiPeak<Float> GetInitialState(
    const mqc3_cloud::program::v1::BosonicState* initial_state) {
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

template <std::floating_point Float>
bosim::State<Float> InitializeState(
    const mqc3_cloud::program::v1::CircuitRepresentation* circuit_repr) {
    using namespace bosim;
    auto modes = std::vector<SingleModeMultiPeak<Float>>();
    for (auto i = 0; i < circuit_repr->initial_states_size(); ++i) {
        const auto& initial_state = circuit_repr->initial_states(i);
        modes.emplace_back(GetInitialState<Float>(&initial_state.bosonic()));
    }
    return State<Float>(modes);
}

template <std::floating_point Float>
void SetState(const bosim::State<Float>& state, mqc3_cloud::program::v1::BosonicState* out) {
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
