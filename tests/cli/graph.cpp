#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <iostream>

#include "bosim/python/manager.h"
#include "bosim/simulate/graph_repr.h"
#include "bosim/simulate/graph_simulate.h"
#include "bosim/simulate/settings.h"

auto ParseGraphRepr(const std::filesystem::path& input_path) {
    auto ifs = std::ifstream{input_path};
    if (!ifs) { throw std::runtime_error{"Failed to open input file: " + input_path.string()}; }

    auto msg = bosim::graph::GraphRepresentation{};
    auto input_stream = google::protobuf::io::IstreamInputStream{&ifs};
    if (!google::protobuf::TextFormat::Parse(&input_stream, &msg)) {
        throw std::runtime_error{fmt::format(
            "Failed to parse input file '{}' as protobuf text format.", input_path.string())};
    }
    return msg;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fmt::print(stderr,
                   "Usage: {} <input_file_path> <output_file_path> <n_shots> <squeezing_level>\n",
                   argv[0]);
        return 1;
    }
    const auto input_path = std::filesystem::path{argv[1]};
    if (!std::filesystem::exists(input_path)) {
        std::cerr << "Input file specified by the path does not exist.\n";
        return 1;
    }
    const auto output_path = std::filesystem::path{argv[2]};
    const auto n_shots = static_cast<std::uint32_t>(std::stoull(argv[3]));
    const auto settings = bosim::SimulateSettings{n_shots, bosim::Backend::CPUSingleThread,
                                                  bosim::StateSaveMethod::None};
    const auto squeezing_level = std::stod(argv[4]);

    const auto env = bosim::python::PythonEnvironment();
    try {
        const auto graph_repr = ParseGraphRepr(input_path);
        const auto result =
            bosim::graph::GraphSimulate<double>(settings, graph_repr, squeezing_level);

        auto ofs = std::ofstream{output_path};
        auto output_stream = google::protobuf::io::OstreamOutputStream{&ofs};
        if (!google::protobuf::TextFormat::Print(result, &output_stream)) {
            std::cerr << "Failed to write message to file.\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
