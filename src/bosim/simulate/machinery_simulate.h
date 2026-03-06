#pragma once

#include "bosim/result.h"
#include "bosim/simulate/machinery_repr.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulate/simulate.h"

#include "mqc3_cloud/program/v1/machinery.pb.h"

namespace bosim::machinery {
using PbMachineryResult = mqc3_cloud::program::v1::MachineryResult;
using PbShotMeasuredValue = mqc3_cloud::program::v1::MachineryResult::ShotMeasuredValue;
using PbMacronodeMeasuredValue = mqc3_cloud::program::v1::MachineryResult::MacronodeMeasuredValue;

template <std::floating_point Float>
PbMachineryResult ExtractMachineryResult(
    const Result<Float>& result, const PhysicalCircuitConfiguration<Float>& machinery_repr) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::ExtractMachineryResult>{};
#endif
    auto pb_result = PbMachineryResult{};
    pb_result.mutable_measured_vals()->Reserve(static_cast<std::int32_t>(result.result.size()));
    for (const auto& shot_result : result.result) {
        auto pb_smv = PbShotMeasuredValue{};
        pb_smv.mutable_measured_vals()->Reserve(
            static_cast<std::int32_t>(machinery_repr.readout_macronodes_indices.size()));
        for (const auto readout_idx : machinery_repr.readout_macronodes_indices) {
            auto pb_mmv = PbMacronodeMeasuredValue{};
            const auto a = static_cast<float>(shot_result.measured_values.at(
                MacronodeOpIdx(readout_idx, MacronodeOpType::HomodyneA)));
            const auto b = static_cast<float>(shot_result.measured_values.at(
                MacronodeOpIdx(readout_idx, MacronodeOpType::HomodyneB)));
            const auto c = static_cast<float>(shot_result.measured_values.at(
                MacronodeOpIdx(readout_idx, MacronodeOpType::HomodyneC)));
            const auto d = static_cast<float>(shot_result.measured_values.at(
                MacronodeOpIdx(readout_idx, MacronodeOpType::HomodyneD)));

            // Converting m to M
            pb_mmv.set_m_a((a - b + c - d) / 2);
            pb_mmv.set_m_b((a + b + c + d) / 2);
            pb_mmv.set_m_c((-a + b + c - d) / 2);
            pb_mmv.set_m_d((-a - b + c + d) / 2);

            // Set index
            pb_mmv.set_index(static_cast<std::uint32_t>(readout_idx));

            pb_smv.mutable_measured_vals()->Add(std::move(pb_mmv));
        }
        pb_result.mutable_measured_vals()->Add(std::move(pb_smv));
    }
    return pb_result;
}

/**
 * @brief Execute machinery simulation.
 *
 * @tparam Float
 * @param settings Simulation settings.
 * @param machinery_repr Machinery representation.
 * @param squeezing_level Squeezing level (in dB) of the x-squeezed modes in the EPR states utilized
 * for gate teleportation. The squeezing level of the p-squeezed resource modes is the oppsite of
 * this.
 * @return std::pair<Circuit<Float>, PbMachineryResult>
 * - Circuit object after the simulation.
 * - Raw result object.
 */
template <std::floating_point Float>
auto MachinerySimulateImpl(const SimulateSettings& settings,
                           const machinery::PhysicalCircuitConfiguration<Float>& machinery_repr,
                           const Float squeezing_level)
    -> std::pair<Circuit<Float>, Result<Float>> {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile_initialization = profiler::Profiler<profiler::Section::InitializeMachineryModes>{};
#endif
    const auto n_local_macronodes = machinery_repr.n_local_macronodes;
    // Set initial state
    auto modes = std::vector<SingleModeMultiPeak<Float>>(5 + n_local_macronodes);
    modes[0] = SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level);
    modes[1] = SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level);
    modes[2] = SingleModeMultiPeak<Float>::GenerateSqueezed(-squeezing_level);
    modes[3] = SingleModeMultiPeak<Float>::GenerateSqueezed(-squeezing_level);
    modes[4] = SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level);
    for (std::size_t i = 5; i < n_local_macronodes + 4; ++i) {
        modes[i] = SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level);
    }
    modes[n_local_macronodes + 4] = SingleModeMultiPeak<Float>::GenerateSqueezed(squeezing_level);
    auto state = State<Float>{modes};
#ifdef BOSIM_BUILD_BENCHMARK
    profile_initialization.End();
#endif

    auto circuit = MachineryCircuit<Float>(machinery_repr, squeezing_level);
    auto result = Simulate<Float>(settings, circuit, state);
    return std::make_pair(std::move(circuit), std::move(result));
}

template <std::floating_point Float>
auto MachinerySimulate(const SimulateSettings& settings,
                       const machinery::PhysicalCircuitConfiguration<Float>& machinery_repr,
                       const Float squeezing_level) -> PbMachineryResult {
    const auto result =
        MachinerySimulateImpl<Float>(settings, machinery_repr, squeezing_level).second;
    return ExtractMachineryResult(result, machinery_repr);
}
}  // namespace bosim::machinery
