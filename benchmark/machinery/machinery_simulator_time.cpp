#include <fmt/format.h>

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#include "bosim/simulate/machinery_repr.h"
#include "bosim/simulate/machinery_simulate.h"

namespace {
auto ParseMachineryRepr(const std::filesystem::path& input_path) {
    auto ifs = std::ifstream{input_path};
    if (!ifs) { throw std::runtime_error{"Could not open input file"}; }

    auto msg = bosim::machinery::PbMachineryRepresentation{};
    auto input_stream = google::protobuf::io::IstreamInputStream{&ifs};
    if (!google::protobuf::TextFormat::Parse(&input_stream, &msg)) {
        throw std::runtime_error{fmt::format(
            "Failed to parse input file '{}' as protobuf text format.", input_path.string())};
    }
    return bosim::machinery::PhysicalCircuitConfiguration<double>{msg};
}

template <std::floating_point Float>
void Benchmark(const std::size_t n_trial, const std::size_t n_warmup,
               const bosim::SimulateSettings& settings,
               const bosim::machinery::PhysicalCircuitConfiguration<Float>& machinery_repr,
               const Float squeezing_level_in_db) {
    for (std::size_t i = 0; i < n_trial + n_warmup; ++i) {
        auto result = bosim::machinery::MachinerySimulate<double>(settings, machinery_repr,
                                                                  squeezing_level_in_db);
        if (i < n_warmup) { bosim::profiler::TimeRecord::GetInstance().Clear(); }
    }
}
}  // namespace

int main(int argc, char** argv) {
    if (argc != 8) {
        fmt::println(stderr,
                     "Usage: {} <input_path> <output_path> <device> <n_cores> <n_local_macronodes> "
                     "<n_steps> <n_shots>",
                     argv[0]);
        return 1;
    }
    const auto input_path = std::filesystem::path{argv[1]};
    const auto output_path = std::string{argv[2]};
    const auto device = std::string{argv[3]};
    const auto n_cores = static_cast<std::size_t>(std::stoi(argv[4]));
    const auto n_local_macronodes = static_cast<std::uint32_t>(std::stoi(argv[5]));
    const auto n_steps = static_cast<std::uint32_t>(std::stoi(argv[6]));
    const auto n_shots = static_cast<std::uint32_t>(std::stoi(argv[7]));

    if (n_local_macronodes < 1) {
        std::cerr << "`n_local_macronodes` must be at least 1.\n";
        return 1;
    }
    if (n_steps < 2) {
        std::cerr << "`n_steps` must be at least 2.\n";
        return 1;
    }

    const auto machinery_repr = ParseMachineryRepr(input_path);

    auto backend = bosim::Backend{};
    if (device == "cpu") {
        backend = bosim::Backend::CPUSingleThread;
    } else if (device == "gpu") {
        backend = bosim::Backend::SingleGPU;
    } else {
        std::cerr << "Invalid device type. Either cpu or gpu is acceptable.\n";
        return 1;
    }

    constexpr std::size_t NTrial = 10;
    constexpr std::size_t NWarmup = 3;
    constexpr auto SqueezingLevel = 10.0;

    const auto settings = bosim::SimulateSettings{n_shots, backend, bosim::StateSaveMethod::None};

    bosim::profiler::TimeRecord::GetInstance().Enable();
    Benchmark(NTrial, NWarmup, settings, machinery_repr, SqueezingLevel);

    std::cout << device << "," << n_cores << "," << n_local_macronodes << "," << n_steps << ","
              << n_shots << ",";
    const auto medians = bosim::profiler::ProfilerTool::Medians();
    bosim::profiler::ProfilerTool::PrintMediansMs(medians);

    auto ofs = std::ofstream{output_path, std::ios::app};
    if (!ofs) {
        std::cerr << fmt::format("Error: Failed to open output file '{}' for appending.\n",
                                 output_path);
        return 1;
    }
    ofs << device << "," << n_cores << "," << n_local_macronodes << "," << n_steps << "," << n_shots
        << ",";
    ofs.flush();
    ofs.close();
    bosim::profiler::ProfilerTool::SaveMediansMs(output_path, medians);

    bosim::profiler::TimeRecord::GetInstance().Clear();

    return 0;
}
