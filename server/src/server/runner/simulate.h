#pragma once

#include <concepts>
#include <curl/curl.h>
#include <zlib.h>

#include "bosim/simulate/circuit_simulate.h"
#include "bosim/simulate/graph_simulate.h"
#include "bosim/simulate/machinery_repr.h"
#include "bosim/simulate/machinery_simulate.h"
#include "bosim/simulate/settings.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"
#include "mqc3_cloud/program/v1/quantum_program.pb.h"

template <std::floating_point Float>
void SimulateCircuit(mqc3_cloud::program::v1::QuantumProgramResult* o_result,
                     const bosim::SimulateSettings& settings,
                     const mqc3_cloud::program::v1::CircuitRepresentation& circuit) {
    bosim::circuit::CircuitSimulate<Float>(o_result, settings, circuit);
}
template <std::floating_point Float>
void SimulateGraph(mqc3_cloud::program::v1::QuantumProgramResult* o_result,
                   const bosim::SimulateSettings& settings, Float resource_squeezing_level,
                   const mqc3_cloud::program::v1::GraphRepresentation& graph) {
    o_result->mutable_graph_result()->CopyFrom(
        bosim::graph::GraphSimulate(settings, graph, resource_squeezing_level));
}
template <std::floating_point Float>
void SimulateMachinery(mqc3_cloud::program::v1::QuantumProgramResult* o_result,
                       const bosim::SimulateSettings& settings, Float resource_squeezing_level,
                       const mqc3_cloud::program::v1::MachineryRepresentation& machinery) {
    const auto config = bosim::machinery::PhysicalCircuitConfiguration<Float>(machinery);
    o_result->mutable_machinery_result()->CopyFrom(
        bosim::machinery::MachinerySimulate(settings, config, resource_squeezing_level));
}
