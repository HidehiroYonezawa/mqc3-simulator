#include "initial_state.h"

int main(int argc, char** argv) {
    if (argc != 5) throw std::invalid_argument("Invalid number of commandline arguments");
    const auto n_cores = static_cast<std::size_t>(std::stoi(argv[1]));
    const auto n_modes = static_cast<std::size_t>(std::stoi(argv[2]));
    const auto n_cats = static_cast<std::size_t>(std::stoi(argv[3]));
    const auto output_file_path = std::string{argv[4]};

    if (n_modes < 2) throw std::invalid_argument("The number of modes must be at least two.");
    if (n_cats > n_modes) {
        throw std::invalid_argument(
            "The number of modes must be no more than the total number of modes.");
    }

    constexpr std::size_t NTrial = 1;
    constexpr std::size_t NWarmup = 0;

    bosim::profiler::TimeRecord::GetInstance().Enable();

    for (std::size_t i = 0; i < NTrial + NWarmup; ++i) {
        auto state = bosim::benchmark::InitialState<double>(n_modes, n_cats);
        if (i < NWarmup) { bosim::profiler::TimeRecord::GetInstance().Clear(); }
    }

    const auto benchmark = std::filesystem::path{__FILE__}.filename().stem().string();
    std::cout << benchmark << ",cpu," << n_cores << "," << n_modes << "," << n_cats << ",";
    bosim::profiler::ProfilerTool::PrintMediansMs();

    auto ofs = std::ofstream{output_file_path, std::ios::app};
    if (!ofs) {
        std::cerr << fmt::format("Error: Failed to open output file '{}' for appending.\n",
                                 output_file_path);
        return 1;
    }
    ofs << benchmark << ",cpu," << n_cores << "," << n_modes << "," << n_cats << ",";
    ofs.flush();
    ofs.close();
    bosim::profiler::ProfilerTool::SaveMediansMs(output_file_path);

    bosim::profiler::TimeRecord::GetInstance().Clear();

    return 0;
}
