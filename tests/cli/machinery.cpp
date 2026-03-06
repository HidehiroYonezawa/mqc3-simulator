#include <filesystem>
#include <fstream>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <iostream>

#include "bosim/simulate/machinery_simulate.h"

bosim::machinery::PbMachineryRepresentation ParseMachineryRepr(
    const std::filesystem::path& input_path) {
    auto ifs = std::ifstream{input_path};
    if (!ifs) { throw std::runtime_error{"Failed to open input file: " + input_path.string()}; }

    auto msg = bosim::machinery::PbMachineryRepresentation{};
    auto input_stream = google::protobuf::io::IstreamInputStream{&ifs};
    if (!google::protobuf::TextFormat::Parse(&input_stream, &msg)) {
        throw std::runtime_error{fmt::format(
            "Failed to parse input file '{}' as protobuf text format.", input_path.string())};
    }

    return msg;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0]
                  << " <input_file_path> <output_file_path> <n_shots> <squeezing_level>\n";
        return 1;
    }
    const auto input_path = std::filesystem::path{argv[1]};
    std::cout << "Input path: " << input_path << "\n";
    if (!std::filesystem::exists(input_path)) {
        std::cerr << "Input file specified by the path does not exist.\n";
        return 1;
    }

    const auto env = bosim::python::PythonEnvironment();
    const auto pb_config = ParseMachineryRepr(input_path);
    const auto config = bosim::machinery::PhysicalCircuitConfiguration<double>{pb_config};

    const std::size_t n_shots = std::stoull(argv[3]);
    const auto settings =
        bosim::SimulateSettings{static_cast<std::uint32_t>(n_shots),
                                bosim::Backend::CPUSingleThread, bosim::StateSaveMethod::None};

    const double squeezing_level = std::stod(argv[4]);
    const auto pb_result =
        bosim::machinery::MachinerySimulate<double>(settings, config, squeezing_level);

    const auto output_path = std::filesystem::path{argv[2]};
    auto ofs = std::ofstream{output_path};
    if (!ofs) { throw std::runtime_error{"Failed to open output file: " + output_path.string()}; }
    auto output_stream = google::protobuf::io::OstreamOutputStream{&ofs};
    if (!google::protobuf::TextFormat::Print(pb_result, &output_stream)) {
        std::cerr << "Failed to write message to file.\n";
        return 1;
    }

    return 0;
}
