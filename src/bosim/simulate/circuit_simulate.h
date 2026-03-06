#pragma once

#include <google/protobuf/repeated_field.h>

#include "bosim/simulate/circuit_repr.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"
#include "mqc3_cloud/program/v1/quantum_program.pb.h"

namespace bosim::circuit {

using mqc3_cloud::program::v1::CircuitRepresentation;
using mqc3_cloud::program::v1::CircuitResult;

/**
 * @brief Simulate a circuit representation.
 *
 * @tparam Float The floating point type.
 * @param settings Simulation settings.
 * @param circuit_repr Circuit representation.
 * @return Simulation result.
 */
template <std::floating_point Float>
auto CircuitSimulate(mqc3_cloud::program::v1::QuantumProgramResult* o_result,
                     const SimulateSettings& settings, const CircuitRepresentation& circuit_repr) {
    auto initial_state = InitializeState<Float>(&circuit_repr);
    auto circuit = CircuitFromProto<Float>(&circuit_repr);

    const auto result = Simulate<Float>(settings, circuit, initial_state);

    // Reserve
    o_result->mutable_circuit_result()->mutable_measured_vals()->Reserve(
        static_cast<std::int32_t>(settings.n_shots));

    for (const auto& shot_result : result.result) {
        auto* o_measured_vals = o_result->mutable_circuit_result()->add_measured_vals();
        SetMeasuredValues<Float>(circuit, shot_result.measured_values, o_measured_vals);
        if (shot_result.state) {
            auto* o_circuit_state = o_result->add_circuit_state();
            SetState<Float>(shot_result.state.value(), o_circuit_state);
        }
    }
}
}  // namespace bosim::circuit
