#include "server/runner/validate.h"

#include <complex>
#include <optional>
#include <stdexcept>

#include "bosim/simulate/settings.h"

#include "server/common/utility.h"
#include "server/runner/common.h"

#include "mqc3_cloud/program/v1/circuit.pb.h"

std::optional<mqc3_cloud::common::v1::ErrorDetail> Validator::ValidateExecutionSettings(
    const mqc3_cloud::scheduler::v1::JobExecutionSettings& execution_settings) const {
    if (!initialized_) {
        throw std::logic_error("Must call 'Initialize' before using this method.");
    }

    // Check format.
    if (format_ == ProgramFormat::UNSPECIFIED) {
        return InvalidProgram::Error("Neither circuit, graph, nor machinery are set");
    }

    // Check n_shots.
    if (n_shots_ <= 0) { return InvalidProgram::Error("'n_shots' is not positive"); }
    if (state_save_method_ == bosim::StateSaveMethod::All &&
        n_shots_ > settings_.max_shots_when_save_all_states) {
        return InvalidProgram::LimitError("'n_shots'", n_shots_,
                                          settings_.max_shots_when_save_all_states);
    } else if (n_shots_ > settings_.max_shots_otherwise) {
        return InvalidProgram::LimitError("'n_shots'", n_shots_, settings_.max_shots_otherwise);
    }

    // Check timeout.
    const auto timeout = Duration(execution_settings.timeout());
    if (settings_.max_timeout_per_role.contains(execution_settings.role())) {
        const auto max_timeout = settings_.max_timeout_per_role.at(execution_settings.role());
        if (timeout > max_timeout) {
            return InvalidProgram::LimitError("'timeout'", timeout.Seconds(),
                                              max_timeout.Seconds());
        }
    }

    return std::nullopt;
}

std::optional<mqc3_cloud::common::v1::ErrorDetail> Validator::ValidateProgram(
    const mqc3_cloud::program::v1::QuantumProgram& program) const {
    if (!initialized_) {
        throw std::logic_error("Must call 'Initialize' before using this method.");
    }

    if (program.has_circuit()) {
        return ValidateCircuit(program.circuit());
    } else if (program.has_graph()) {
        return ValidateGraph(program.graph());
    } else if (program.has_machinery()) {
        return ValidateMachinery(program.machinery());
    }
    return std::nullopt;
}

std::optional<mqc3_cloud::common::v1::ErrorDetail> Validator::ValidateCircuit(
    const mqc3_cloud::program::v1::CircuitRepresentation& circuit) const {
    if (!initialized_) {
        throw std::logic_error("Must call 'Initialize' before using this method.");
    }

    const auto n_modes = circuit.n_modes();
    const auto n_ops = static_cast<std::uint32_t>(circuit.operations_size());

    if (n_modes > settings_.max_n_modes) {
        return InvalidProgram::LimitError("'n_modes'", n_modes, settings_.max_n_modes);
    }
    if (n_ops > settings_.max_n_ops) {
        return InvalidProgram::LimitError("Number of 'operations'", n_ops, settings_.max_n_ops);
    }
    const auto n_measurement_ops = std::ranges::count(
        circuit.operations(), mqc3_cloud::program::v1::CircuitOperation::OPERATION_TYPE_MEASUREMENT,
        [](const mqc3_cloud::program::v1::CircuitOperation& op) { return op.type(); });
    if (n_measurement_ops > settings_.max_n_measurement_ops) {
        return InvalidProgram::LimitError("Number of 'OPERATION_TYPE_MEASUREMENT' in 'operations'",
                                          n_measurement_ops, settings_.max_n_measurement_ops);
    }
    if (const auto prod =
            static_cast<std::uint64_t>(n_measurement_ops) * static_cast<std::uint64_t>(n_shots_);
        prod > settings_.max_prod_shot_readout) {
        return InvalidProgram::LimitError(
            "Product of number of measurements operations and 'n_shots'", prod,
            settings_.max_prod_shot_readout);
    }
    auto peak_prod = std::uint64_t{1};
    for (const auto& initial_state : circuit.initial_states()) {
        if (initial_state.has_squeezed()) {
            return InvalidProgram::Error("Found not bosonic initial state");
        }
        const auto& state = initial_state.bosonic();
        const auto n_peaks = static_cast<std::uint64_t>(state.coeffs_size());

        if (n_peaks == 0) {
            return InvalidProgram::Error("Found invalid initial state: number of peaks is zero");
        }

        if (peak_prod > settings_.max_prod_n_peaks / n_peaks) {  // overflow-safe multiplication
            return InvalidProgram::Error(
                fmt::format("Product of peak counts exceeds the allowed limit ({})",
                            settings_.max_prod_n_peaks));
        }

        peak_prod *= n_peaks;
    }
    // Estimate memory usage (in bytes):
    const std::uint64_t n_quadratures = 2 * static_cast<std::uint64_t>(n_modes);
    const std::uint64_t covariance_bytes =
        sizeof(double) * n_quadratures * n_quadratures * peak_prod;
    const std::uint64_t mean_bytes = sizeof(std::complex<double>) * n_quadratures * peak_prod;
    const std::uint64_t memory_usage = covariance_bytes + mean_bytes;
    if (memory_usage > settings_.max_memory_usage) {
        const auto memory_usage_gib = static_cast<double>(memory_usage) / (1024 * 1024 * 1024);
        const auto max_memory_gib =
            static_cast<double>(settings_.max_memory_usage) / (1024 * 1024 * 1024);
        return InvalidProgram::LimitError("Estimated memory usage", memory_usage_gib,
                                          max_memory_gib);
    }

    return std::nullopt;
}

std::optional<mqc3_cloud::common::v1::ErrorDetail> Validator::ValidateGraph(
    const mqc3_cloud::program::v1::GraphRepresentation& graph) const {
    if (!initialized_) {
        throw std::logic_error("Must call 'Initialize' before using this method.");
    }

    const auto n_local_macronodes = graph.n_local_macronodes();
    const auto n_steps = graph.n_steps();

    if (n_local_macronodes == 0) {
        return InvalidProgram::Error("'n_local_macronodes' of graph is zero");
    }
    if (n_local_macronodes > settings_.max_n_local_macronodes_graph) {
        return InvalidProgram::LimitError("'n_local_macronodes' of graph", n_local_macronodes,
                                          settings_.max_n_local_macronodes_graph);
    }
    if (n_steps == 0) { return InvalidProgram::Error("'n_steps' of graph is zero"); }
    if (n_steps > settings_.max_n_steps_graph) {
        return InvalidProgram::LimitError("'n_steps' of graph", n_steps,
                                          settings_.max_n_steps_graph);
    }
    const auto n_measurement_ops = std::ranges::count(
        graph.operations(), mqc3_cloud::program::v1::GraphOperation::OPERATION_TYPE_MEASUREMENT,
        [](const mqc3_cloud::program::v1::GraphOperation& op) { return op.type(); });
    if (const auto prod =
            static_cast<std::uint64_t>(n_measurement_ops) * static_cast<std::uint64_t>(n_shots_);
        prod > settings_.max_prod_shot_readout) {
        return InvalidProgram::LimitError(
            "Product of number of measurements operations and 'n_shots'", prod,
            settings_.max_prod_shot_readout);
    }

    return std::nullopt;
}

std::optional<mqc3_cloud::common::v1::ErrorDetail> Validator::ValidateMachinery(
    const mqc3_cloud::program::v1::MachineryRepresentation& machinery) const {
    if (!initialized_) {
        throw std::logic_error("Must call 'Initialize' before using this method.");
    }

    const auto n_local_macronodes = machinery.n_local_macronodes();
    const auto n_steps = machinery.n_steps();

    if (n_local_macronodes == 0) {
        return InvalidProgram::Error("'n_local_macronodes' of machinery is zero");
    }
    if (n_local_macronodes > settings_.max_n_local_macronodes_machinery) {
        return InvalidProgram::LimitError("'n_local_macronodes' of machinery", n_local_macronodes,
                                          settings_.max_n_local_macronodes_machinery);
    }
    if (n_steps == 0) { return InvalidProgram::Error("'n_steps' of machinery is zero"); }
    if (n_steps > settings_.max_n_steps_graph) {
        return InvalidProgram::LimitError("'n_steps' of machinery", n_steps,
                                          settings_.max_n_steps_graph);
    }
    if (const auto prod = static_cast<std::uint64_t>(machinery.readout_macronodes_indices_size()) *
                          static_cast<std::uint64_t>(n_shots_);
        prod > settings_.max_prod_shot_readout) {
        return InvalidProgram::LimitError("Product of number of readout macronodes and 'n_shots'",
                                          prod, settings_.max_prod_shot_readout);
    }

    return std::nullopt;
}
