#include <fmt/format.h>

#include "bosim/base/timer.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        fmt::println(stderr, "Usage: {} <output_file_path>", argv[0]);
        return 1;
    }
    const auto output_file_path = std::string{argv[1]};

    std::cout << "device,n_cores,n_local_macronodes,n_steps,n_shots,";
    bosim::profiler::ProfilerTool::PrintSectionNames();

    auto ofs = std::ofstream{output_file_path};
    if (!ofs) {
        std::cerr << fmt::format("Error: Failed to open output file '{}'.\n", output_file_path);
        return 1;
    }
    ofs << "device,n_cores,n_local_macronodes,n_steps,n_shots,";
    ofs.flush();
    ofs.close();
    bosim::profiler::ProfilerTool::SaveSectionNames(output_file_path);

    return 0;
}
