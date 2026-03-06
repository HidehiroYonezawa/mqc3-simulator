#include "bosim/base/timer.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Invalid number of commandline arguments.\n";
        return 1;
    }
    const auto output_file_path = std::string{argv[1]};

    std::cout << "benchmark,device,n_cores,n_modes,n_cats,";
    bosim::profiler::ProfilerTool::PrintSectionNames();

    auto ofs = std::ofstream{output_file_path};
    if (!ofs) {
        std::cerr << fmt::format("Error: Failed to open output file '{}'.\n", output_file_path);
        return 1;
    }
    ofs << "benchmark,device,n_cores,n_modes,n_cats,";
    ofs.flush();
    ofs.close();
    bosim::profiler::ProfilerTool::SaveSectionNames(output_file_path);

    return 0;
}
