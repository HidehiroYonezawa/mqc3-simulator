#include "circuit.h"

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <iostream>

#include "bosim/python/manager.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"

auto ParseCircuitRepr(const std::filesystem::path& input_path) {
    auto ifs = std::ifstream{input_path};
    if (!ifs) { throw std::runtime_error{"Failed to open input file: " + input_path.string()}; }

    auto msg = mqc3_cloud::program::v1::CircuitRepresentation{};
    auto input_stream = google::protobuf::io::IstreamInputStream{&ifs};
    if (!google::protobuf::TextFormat::Parse(&input_stream, &msg)) {
        throw std::runtime_error{fmt::format(
            "Failed to parse input file '{}' as protobuf text format.", input_path.string())};
    }
    return msg;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        fmt::print(stderr, "Usage: {} <input_file_path> <output_file_path>\n", argv[0]);
        return 1;
    }
    const auto input_path = std::filesystem::path{argv[1]};
    if (!std::filesystem::exists(input_path)) {
        std::cerr << "Input file specified by the path does not exist.\n";
        return 1;
    }
    const auto output_path = std::filesystem::path{argv[2]};
    const auto settings =
        bosim::SimulateSettings{1, bosim::Backend::CPUSingleThread, bosim::StateSaveMethod::All};

    const auto env = bosim::python::PythonEnvironment();
    try {
        const auto pb_circuit = ParseCircuitRepr(input_path);
        auto circuit = InitializeCircuit<double>(&pb_circuit);
        const auto initial_state = InitializeState<double>(&pb_circuit);

        const auto result = bosim::Simulate(settings, circuit, initial_state);

        if (result.result.size() != 1) {
            std::cerr << "The number of shot results should be 1.\n";
            return 1;
        }

        const auto state = result.result.back().state;
        if (state.has_value()) {
            auto pb_state = mqc3_cloud::program::v1::BosonicState{};
            SetState(state.value(), &pb_state);

            auto ofs = std::ofstream{output_path};
            auto output_stream = google::protobuf::io::OstreamOutputStream{&ofs};
            if (!google::protobuf::TextFormat::Print(pb_state, &output_stream)) {
                std::cerr << "Failed to write message to file.\n";
                return 1;
            }
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
