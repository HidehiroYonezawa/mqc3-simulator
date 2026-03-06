#include "common.h"
#include "initial_state.h"

auto BSOpTimeMeasurementConfig(const std::size_t n_modes, const std::size_t n_cats)
    -> std::pair<bosim::Circuit<double>, bosim::State<double>> {
    auto gen = std::mt19937{0};  // NOLINT(cert-msc32-c, cert-msc51-cpp)
    auto angle_dist = std::uniform_real_distribution<double>{-std::numbers::pi_v<double>,
                                                             std::numbers::pi_v<double>};
    auto mode_dist = std::uniform_int_distribution<std::size_t>{0, n_modes - 1};

    // Circuit
    const std::size_t n_ops = n_modes * 10;
    auto circuit = bosim::Circuit<double>{};
    for (std::size_t i = 0; i < n_ops; ++i) {
        while (true) {
            const auto mode0 = mode_dist(gen);
            const auto mode1 = mode_dist(gen);
            if (mode0 == mode1) continue;
            circuit.EmplaceOperation<bosim::operation::util::BeamSplitterStd<double>>(
                mode0, mode1, angle_dist(gen), angle_dist(gen));
            break;
        }
    }

    return {std::move(circuit), bosim::benchmark::InitialState<double>(n_modes, n_cats)};
}

int main(int argc, char** argv) {
    if (argc < 6) throw std::invalid_argument("Invalid number of commandline arguments.");
    const auto device = std::string{argv[1]};
    const auto n_cores = static_cast<std::size_t>(std::stoi(argv[2]));
    const auto n_modes = static_cast<std::size_t>(std::stoi(argv[3]));
    const auto n_cats = static_cast<std::size_t>(std::stoi(argv[4]));
    const auto output_file_path = std::string{argv[5]};
    const auto n_shots = (argc >= 7) ? static_cast<std::uint32_t>(std::stoi(argv[6])) : 1;
    const auto n_warmup = (argc >= 8) ? static_cast<std::size_t>(std::stoi(argv[7])) : 0;
    const auto n_trial = (argc >= 9) ? static_cast<std::size_t>(std::stoi(argv[8])) : 1;

    if (n_modes < 2) throw std::invalid_argument("The number of modes must be at least two.");
    if (n_cats > n_modes) {
        throw std::invalid_argument(
            "The number of modes must be no more than the total number of modes.");
    }

    auto [circuit, state] = BSOpTimeMeasurementConfig(n_modes, n_cats);
    auto backend = bosim::Backend{};
    if (device == "cpu") {
        backend = bosim::Backend::CPUMultiThread;
    } else if (device == "cpu_shot_level") {
        backend = bosim::Backend::CPUMultiThreadShotLevel;
    } else if (device == "cpu_peak_level") {
        backend = bosim::Backend::CPUMultiThreadPeakLevel;
    } else if (device == "single_gpu") {
        backend = bosim::Backend::SingleGPU;
    } else if (device == "single_gpu_single_thread") {
        backend = bosim::Backend::SingleGPUSingleThread;
    } else if (device == "multi_gpu") {
        backend = bosim::Backend::MultiGPU;
    } else {
        throw std::invalid_argument("Invalid device type. Either cpu or gpu is acceptable.");
    }
    const auto settings = bosim::SimulateSettings{n_shots, backend, bosim::StateSaveMethod::None};

    bosim::profiler::TimeRecord::GetInstance().Enable();
    bosim::benchmark::Benchmark<double>(n_trial, n_warmup, settings, circuit, state);

    const auto benchmark = std::filesystem::path{__FILE__}.filename().stem().string();

    std::cout << benchmark << "," << device << "," << n_cores << "," << n_modes << "," << n_cats
              << ",";
    bosim::profiler::ProfilerTool::PrintMediansMs();

    auto ofs = std::ofstream{output_file_path, std::ios::app};
    if (!ofs) {
        std::cerr << fmt::format("Error: Failed to open output file '{}' for appending.\n",
                                 output_file_path);
        return 1;
    }
    ofs << benchmark << "," << device << "," << n_cores << "," << n_modes << "," << n_cats << ",";
    ofs.flush();
    ofs.close();
    bosim::profiler::ProfilerTool::SaveMediansMs(output_file_path);

    bosim::profiler::TimeRecord::GetInstance().Clear();

    return 0;
}
