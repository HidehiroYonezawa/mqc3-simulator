#pragma once

#include <cstdint>

#include "bosim/simulate/settings.h"

#include "mqc3_cloud/program/v1/quantum_program.pb.h"
#include "mqc3_cloud/scheduler/v1/job.pb.h"

enum class ProgramFormat : std::uint8_t { UNSPECIFIED, CIRCUIT, GRAPH, MACHINERY };

inline ProgramFormat GetProgramFormat(const mqc3_cloud::program::v1::QuantumProgram& program) {
    if (program.has_circuit()) {
        return ProgramFormat::CIRCUIT;
    } else if (program.has_graph()) {
        return ProgramFormat::GRAPH;
    } else if (program.has_machinery()) {
        return ProgramFormat::MACHINERY;
    }
    return ProgramFormat::UNSPECIFIED;
}

inline auto GetStateSaveMethod(const mqc3_cloud::scheduler::v1::JobStateSavePolicy& policy) {
    if (policy == mqc3_cloud::scheduler::v1::JobStateSavePolicy::JOB_STATE_SAVE_POLICY_ALL) {
        return bosim::StateSaveMethod::All;
    }
    if (policy == mqc3_cloud::scheduler::v1::JobStateSavePolicy::JOB_STATE_SAVE_POLICY_FIRST_ONLY) {
        return bosim::StateSaveMethod::FirstOnly;
    }
    return bosim::StateSaveMethod::None;
}

/**
 * @brief Get bosim backend from server runner's backend.
 *
 * @details This function maps a user-facing backend string to a specific `bosim::Backend` enum
 * value. The concept of "backend" has three distinct uses within this context:
 * 1. **bosim Library Backend:** This refers to the `bosim::Backend` enum, which determines the
 * specific hardware (CPU or GPU) and configuration (single or multi-threaded/GPU) used for the
 * simulation.
 * 2. **User-Input Backend:** This is the backend string provided by the user, which may allow for
 * ambiguous or user-friendly names (e.g., "gpu" could map to "single_gpu"). The simulation server's
 * runner interprets this string to select an appropriate server.
 * 3. **Server Runner's Backend:** This is a string representation of the `bosim::Backend` used by
 * the server runner. Each user job is assigned to a specific server runner based on the user's
 * input. The server runner's backend string is kept as a string because it is later returned to the
 * user as the "actual_backend" used for their job.
 *
 * @param backend The backend string provided by the user.
 * @return bosim::Backend The `bosim::Backend` enum corresponding to the input string.
 */
inline bosim::Backend GetBackend(const std::string& server_runner_backend) {
    // The following branch condition must
    if (server_runner_backend == "cpu") {
        return bosim::Backend::CPUMultiThread;
    } else if (server_runner_backend == "single_gpu") {
        return bosim::Backend::SingleGPU;
    } else if (server_runner_backend == "multi_gpu") {
        return bosim::Backend::MultiGPU;
    }
    return bosim::Backend::CPUMultiThreadShotLevel;
}
