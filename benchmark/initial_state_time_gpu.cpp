// Performance evaluation of handling initial states comprised of multiple, uncorrelated modes on
// the GPU. The initial state is prepared as a sequence of single modes. We compare performance when
// directly feeding this sequence to the simulator function versus converting it into a 'State'
// object first. The former approach is expected to be faster because the covariance matrix will be
// treated as a sparse matrix.

#include "bosim/circuit.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"
#include "bosim/state.h"

template <std::floating_point Float>
auto InitialModes(const std::size_t n_modes, const std::size_t n_cats) {
    auto gen = std::mt19937{0};  // NOLINT(cert-msc32-c, cert-msc51-cpp)
    auto angle_dist = std::uniform_real_distribution<Float>{-std::numbers::pi_v<Float>,
                                                            std::numbers::pi_v<Float>};

    const std::size_t n_squeezed_states = n_modes - n_cats;
    auto modes = std::vector<bosim::SingleModeMultiPeak<Float>>{};
    for (std::size_t i = 0; i < n_cats; ++i) {
        modes.push_back(bosim::SingleModeMultiPeak<Float>::GenerateCat(
            std::complex<Float>{std::polar(1.0, angle_dist(gen))}, 0));
    }
    for (std::size_t i = 0; i < n_squeezed_states; ++i) {
        modes.push_back(bosim::SingleModeMultiPeak<Float>::GenerateSqueezed(
            5, angle_dist(gen), std::complex<Float>{0, 0}));
    }
    return modes;
}

int main(int argc, char** argv) {
    if (argc != 6) throw std::invalid_argument("Invalid number of commandline arguments");
    const auto constructor = std::string{argv[1]};  // 'state' or 'modes'
    const auto n_cores = static_cast<std::size_t>(std::stoi(argv[2]));
    const auto n_modes = static_cast<std::size_t>(std::stoi(argv[3]));
    const auto n_cats = static_cast<std::size_t>(std::stoi(argv[4]));
    const auto output_file_name = std::string{argv[5]};

    if (n_modes < 2) throw std::invalid_argument("The number of modes must be at least two.");
    if (n_cats > n_modes) {
        throw std::invalid_argument(
            "The number of modes must be no more than the total number of modes.");
    }

    constexpr std::size_t NTrial = 1;
    constexpr std::size_t NWarmup = 0;

    const auto settings =
        bosim::SimulateSettings{1, bosim::Backend::SingleGPU, bosim::StateSaveMethod::None};
    auto circuit = bosim::Circuit<double>();
    const auto modes = InitialModes<double>(n_modes, n_cats);

    bosim::profiler::TimeRecord::GetInstance().Enable();

    if (constructor == "state") {
        const auto state = bosim::State(modes);

        for (std::size_t i = 0; i < NTrial + NWarmup; ++i) {
            bosim::Simulate<double>(settings, circuit, state);
            if (i < NWarmup) { bosim::profiler::TimeRecord::GetInstance().Clear(); }
        }
    } else if (constructor == "modes") {
        for (std::size_t i = 0; i < NTrial + NWarmup; ++i) {
            bosim::Simulate<double>(settings, circuit, modes);
            if (i < NWarmup) { bosim::profiler::TimeRecord::GetInstance().Clear(); }
        }
    } else {
        throw std::invalid_argument("Invalid state type. Either 'state' or 'modes' is acceptable.");
    }

    const auto benchmark = std::filesystem::path{__FILE__}.filename().stem().string();
    std::cout << benchmark << "," << constructor << "," << n_cores << "," << n_modes << ","
              << n_cats << ",";
    bosim::profiler::ProfilerTool::PrintMediansMs();

    auto ofs = std::ofstream{output_file_name, std::ios::app};
    if (!ofs) {
        std::cerr << fmt::format("Error: Failed to open output file '{}' for appending.\n",
                                 output_file_name);
        return 1;
    }
    ofs << benchmark << "," << constructor << "," << n_cores << "," << n_modes << "," << n_cats
        << ",";
    ofs.flush();
    ofs.close();
    bosim::profiler::ProfilerTool::SaveMediansMs(output_file_name);

    bosim::profiler::TimeRecord::GetInstance().Clear();

    return 0;
}
