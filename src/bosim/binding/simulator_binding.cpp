#include <nanobind/nanobind.h>
#include <nanobind/stl/string_view.h>

#include "bosim/simulate/circuit_simulate.h"
#include "bosim/simulate/graph_simulate.h"
#include "bosim/simulate/machinery_repr.h"
#include "bosim/simulate/machinery_simulate.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"
#include "mqc3_cloud/program/v1/graph.pb.h"
#include "mqc3_cloud/program/v1/machinery.pb.h"

namespace bosim::python {
namespace {
auto ConvertToBackend(std::string_view backend_str) -> bosim::Backend {
    if (backend_str == "single_cpu") return bosim::Backend::CPUSingleThread;
    if (backend_str == "multi_cpu") return bosim::Backend::CPUMultiThread;
    if (backend_str == "multi_cpu_peak") return bosim::Backend::CPUMultiThreadPeakLevel;
    if (backend_str == "multi_cpu_shot") return bosim::Backend::CPUMultiThreadShotLevel;
    if (backend_str == "single_gpu") return bosim::Backend::SingleGPU;
    if (backend_str == "multi_gpu") return bosim::Backend::MultiGPU;
    throw std::invalid_argument(fmt::format("Failed to convert '{}' to a backend.", backend_str));
}

auto ConvertToStateSaveMethod(std::string_view save_state_method_str) -> bosim::StateSaveMethod {
    if (save_state_method_str == "first_only") return bosim::StateSaveMethod::FirstOnly;
    if (save_state_method_str == "all") return bosim::StateSaveMethod::All;
    if (save_state_method_str == "none") return bosim::StateSaveMethod::None;
    throw std::invalid_argument(fmt::format("Failed to convert '{}' to a method for saving states.",
                                            save_state_method_str));
}

auto ConstructSimulateSettings(const std::int32_t n_shots, std::string_view backend_str,
                               std::string_view state_save_method_str = "none") {
    return bosim::SimulateSettings{
        .n_shots = static_cast<std::uint32_t>(n_shots),
        .backend = ConvertToBackend(backend_str),
        .save_state_method = ConvertToStateSaveMethod(state_save_method_str)};
}

template <typename T>
concept SerializableProto = requires(T t) {
    { t.SerializeAsString() } -> std::same_as<std::string>;
};

template <SerializableProto T>
auto SerializeProto(const T& proto) {
    const auto proto_str = proto.SerializeAsString();
    if (proto_str.empty()) { throw std::runtime_error("Failed to serialize proto."); }
    return nanobind::bytes(proto_str.data(), proto_str.size());
}
}  // namespace

NB_MODULE(_mqc3_simulator_impl, m) {
    m.def(
        "simulate_circuit",
        [](const nanobind::bytes& circuit_repr_bytes, std::string_view backend_str,
           const std::int32_t n_shots, std::string_view state_save_method_str) -> nanobind::bytes {
            const auto settings =
                ConstructSimulateSettings(n_shots, backend_str, state_save_method_str);

            const auto circuit_repr = [](const nanobind::bytes& circuit_repr_bytes) {
                mqc3_cloud::program::v1::CircuitRepresentation circuit_repr;
                if (!circuit_repr.ParseFromArray(
                        circuit_repr_bytes.data(),
                        static_cast<std::int32_t>(circuit_repr_bytes.size()))) {
                    throw std::invalid_argument(
                        "Failed to parse the circuit representation proto.");
                }
                return circuit_repr;
            }(circuit_repr_bytes);

            const auto result_proto =
                [](const bosim::SimulateSettings& settings,
                   const mqc3_cloud::program::v1::CircuitRepresentation& circuit_repr) {
                    const auto gil_release = nanobind::gil_scoped_release();
                    mqc3_cloud::program::v1::QuantumProgramResult result_proto;
                    bosim::circuit::CircuitSimulate<double>(&result_proto, settings, circuit_repr);
                    return result_proto;
                }(settings, circuit_repr);
            return SerializeProto(result_proto);
        },
        R"(
            Simulate a circuit representation.

            Args:
                circuit_repr_bytes (bytes): Serialized circuit representation.
                backend_str (str): Simulation backend.
                    Valid options: "single_cpu", "multi_cpu", "multi_cpu_peak", "multi_cpu_shot", "single_gpu", "multi_gpu".
                n_shots (int): Number of shots. Must be a 32-bit integer.
                state_save_method_str (str): Method for saving shot states.
                    Valid options: "first_only", "all", "none".

            Returns:
                bytes: Simulation result in `QuantumProgramResult` format.

            Raises:
                ValueError: If parsing of the serialized circuit representation fails.
           )");

    m.def(
        "simulate_graph",
        [](const nanobind::bytes& graph_repr_bytes, std::string_view backend_str,
           const std::int32_t n_shots, const double squeezing_level) -> nanobind::bytes {
            const auto settings = ConstructSimulateSettings(n_shots, backend_str);

            const auto graph_repr = [](const nanobind::bytes& graph_repr_bytes) {
                mqc3_cloud::program::v1::GraphRepresentation graph_repr;
                if (!graph_repr.ParseFromArray(
                        graph_repr_bytes.data(),
                        static_cast<std::int32_t>(graph_repr_bytes.size()))) {
                    throw std::invalid_argument("Failed to parse the graph representation proto.");
                }
                return graph_repr;
            }(graph_repr_bytes);

            const auto result_proto =
                [](const bosim::SimulateSettings& settings,
                   const mqc3_cloud::program::v1::GraphRepresentation& graph_repr,
                   const double squeezing_level) {
                    const auto gil_release = nanobind::gil_scoped_release();
                    return bosim::graph::GraphSimulate(settings, graph_repr, squeezing_level);
                }(settings, graph_repr, squeezing_level);
            return SerializeProto(result_proto);
        },
        R"(
           Simulate a graph representation.

            Args:
                graph_repr_bytes (bytes): Serialized graph representation.
                backend_str (str): Simulation backend.
                    Valid options: "single_cpu", "multi_cpu", "multi_cpu_peak", "multi_cpu_shot", "single_gpu", "multi_gpu".
                n_shots (int): Number of shots. Must be a 32-bit integer.
                squeezing_level (float): Resource squeezing level in dB.

            Returns:
                bytes: Simulation result in `GraphResult` format.

            Raises:
                ValueError: If parsing of the serialized graph representation fails.
    )");

    m.def(
        "simulate_machinery",
        [](const nanobind::bytes& machinery_repr_bytes, std::string_view backend_str,
           const std::int32_t n_shots, const double squeezing_level) -> nanobind::bytes {
            const auto settings = ConstructSimulateSettings(n_shots, backend_str);

            const auto machinery_repr = [](const nanobind::bytes& machinery_repr_bytes) {
                mqc3_cloud::program::v1::MachineryRepresentation machinery_repr;
                if (!machinery_repr.ParseFromArray(
                        machinery_repr_bytes.data(),
                        static_cast<std::int32_t>(machinery_repr_bytes.size()))) {
                    throw std::invalid_argument(
                        "Failed to parse the machinery representation proto.");
                }
                return bosim::machinery::PhysicalCircuitConfiguration<double>(machinery_repr);
            }(machinery_repr_bytes);

            const auto result_proto =
                [](const bosim::SimulateSettings& settings,
                   const bosim::machinery::PhysicalCircuitConfiguration<double>& machinery_repr,
                   const double squeezing_level) {
                    const auto gil_release = nanobind::gil_scoped_release();
                    return bosim::machinery::MachinerySimulate(settings, machinery_repr,
                                                               squeezing_level);
                }(settings, machinery_repr, squeezing_level);
            return SerializeProto(result_proto);
        },
        R"(
           Simulate a machinery representation.

            Args:
                machinery_repr_bytes (bytes): Serialized machinery representation.
                backend_str (str): Simulation backend.
                    Valid options: "single_cpu", "multi_cpu", "multi_cpu_peak", "multi_cpu_shot", "single_gpu", "multi_gpu".
                n_shots (int): Number of shots. Must be a 32-bit integer.
                squeezing_level (float): Squeezing level in dB of the x-squeezed resource modes.

            Returns:
                bytes: Simulation result in `MachineryResult` format.

            Raises:
                ValueError: If parsing of the serialized machinery representation fails.
        )");
}
}  // namespace bosim::python
