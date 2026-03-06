#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>

#include "bosim/simulate/settings.h"

#include "server/common/error_code.h"
#include "server/common/message_manager.h"
#include "server/common/utility.h"
#include "server/runner/common.h"

#include "mqc3_cloud/common/v1/error_detail.pb.h"
#include "mqc3_cloud/program/v1/circuit.pb.h"
#include "mqc3_cloud/program/v1/quantum_program.pb.h"
#include "mqc3_cloud/scheduler/v1/job.pb.h"

struct ValidateSettings {
    std::int32_t max_shots_when_save_all_states;
    std::int32_t max_shots_otherwise;
    std::uint64_t max_prod_shot_readout;
    std::unordered_map<std::string, Duration> max_timeout_per_role;

    // Circuit representation
    std::uint32_t max_n_modes;
    std::uint32_t max_n_ops;
    std::uint32_t max_n_measurement_ops;
    std::uint32_t max_prod_n_peaks;
    std::uint64_t max_memory_usage;

    // Graph representation
    std::uint32_t max_n_local_macronodes_graph;
    std::uint32_t max_n_steps_graph;

    // Machinery representation
    std::uint32_t max_n_local_macronodes_machinery;
    std::uint32_t max_n_steps_machinery;
};

struct InvalidProgram {
    using ErrorDetail = mqc3_cloud::common::v1::ErrorDetail;

    static ErrorDetail Error(const std::string& reason) {
        return MessageManager::GetMessage(INVALID_PROGRAM, fmt::arg("reason", reason)).ToError();
    }

    template <typename A, typename E>
    static ErrorDetail MatchError(const std::string& name, A&& actual, E&& expected) {
        return Error(fmt::format("{} ({}) does not match expected value ({})", name,
                                 std::forward<A>(actual), std::forward<E>(expected)));
    }
    template <typename A, typename L>
    static ErrorDetail LimitError(const std::string& name, A&& actual, L&& limit) {
        return Error(fmt::format("{} ({}) exceeds the allowed limit ({})", name,
                                 std::forward<A>(actual), std::forward<L>(limit)));
    }
    template <typename L, typename R>
    static ErrorDetail OrderError(const std::string& lhs_name, L&& lhs, const std::string& rhs_name,
                                  R&& rhs) {
        return Error(fmt::format("{} ({}) exceeds {} ({})", lhs_name, std::forward<L>(lhs),
                                 rhs_name, std::forward<R>(rhs)));
    }
    static ErrorDetail IndexError(const std::string& name, std::int32_t idx, std::int32_t size) {
        return Error(fmt::format("{} ({}) is out of range [0, {}]", name, idx, size));
    }
};

class Validator {
public:
    explicit Validator(const ValidateSettings& settings) : settings_{settings} {}

    void Initialize(const mqc3_cloud::program::v1::QuantumProgram& program,
                    const mqc3_cloud::scheduler::v1::JobExecutionSettings& execution_settings) {
        n_shots_ = execution_settings.n_shots();
        format_ = GetProgramFormat(program);
        state_save_method_ = ::GetStateSaveMethod(execution_settings.state_save_policy());

        if (format_ == ProgramFormat::GRAPH || format_ == ProgramFormat::MACHINERY) {
            state_save_method_ = bosim::StateSaveMethod::None;
        }

        initialized_ = true;
    }

    void Clear() {
        initialized_ = false;
        n_shots_ = 0;
        format_ = ProgramFormat::UNSPECIFIED;
        state_save_method_ = bosim::StateSaveMethod::None;
    }

    [[nodiscard]] std::int32_t GetNShots() const { return n_shots_; }
    [[nodiscard]] ProgramFormat GetFormat() const { return format_; }
    [[nodiscard]] bosim::StateSaveMethod GetStateSaveMethod() const { return state_save_method_; }

    std::optional<mqc3_cloud::common::v1::ErrorDetail> ValidateExecutionSettings(
        const mqc3_cloud::scheduler::v1::JobExecutionSettings& execution_settings) const;

    std::optional<mqc3_cloud::common::v1::ErrorDetail> ValidateProgram(
        const mqc3_cloud::program::v1::QuantumProgram& program) const;

    std::optional<mqc3_cloud::common::v1::ErrorDetail> ValidateCircuit(
        const mqc3_cloud::program::v1::CircuitRepresentation& circuit) const;

    std::optional<mqc3_cloud::common::v1::ErrorDetail> ValidateGraph(
        const mqc3_cloud::program::v1::GraphRepresentation& graph) const;

    std::optional<mqc3_cloud::common::v1::ErrorDetail> ValidateMachinery(
        const mqc3_cloud::program::v1::MachineryRepresentation& machinery) const;

private:
    const ValidateSettings settings_;
    bool initialized_ = false;

    std::int32_t n_shots_ = 0;
    ProgramFormat format_ = ProgramFormat::UNSPECIFIED;
    bosim::StateSaveMethod state_save_method_ = bosim::StateSaveMethod::None;
};
