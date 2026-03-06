#pragma once

#include <taskflow/taskflow.hpp>

#include <chrono>
#include <future>
#include <thread>

#include "bosim/circuit.h"
#include "bosim/result.h"
#include "bosim/simulate/settings.h"
#include "bosim/simulator.h"
#include "bosim/state.h"

namespace bosim {
template <std::floating_point Float, typename Clock, typename Duration>
tf::Taskflow SimulateCPUMultiThreadImpl(const EndTime<Clock, Duration>& end_time,
                                        const std::uint64_t seed, const std::uint64_t shot,
                                        const Circuit<Float>& circuit, const State<Float>& state,
                                        const bool save_state, Simulator<Float>& simulator) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateCPUMultiThreadImpl>{shot};
#endif
    auto taskflow = tf::Taskflow{};
    taskflow.emplace([&end_time, &state, seed, shot, &circuit, &simulator,
                      save_state](tf::Subflow& subflow) {
#ifdef BOSIM_BUILD_BENCHMARK
        auto profile = profiler::Profiler<profiler::Section::ShotLevelTaskflow>{shot};
#endif
        RaiseErrorIfTimedout(end_time);

        auto state_copy = state;
        const auto num_peaks = state_copy.NumPeaks();
        state_copy.SetSeed(seed);
        state_copy.SetShot(shot);
        auto circuit_copy = circuit.Copy();
        const auto num_ops = circuit_copy.GetMutOperations().size();

        bool previous_is_measurement = false;
        auto op_tasks = std::vector<std::vector<tf::Task>>(num_ops);
        auto rot_tasks = std::vector<std::vector<tf::Task>>();
        rot_tasks.reserve(num_ops);
        auto postselection_tasks = std::vector<std::vector<tf::Task>>();
        postselection_tasks.reserve(num_ops);

        auto result = typename operation::HomodyneSingleModeX<Float>::SamplingResult();

#ifdef BOSIM_BUILD_BENCHMARK
        auto profile_op =
            profiler::Profiler<profiler::Section::ShotLevelTaskflowOperationLoop>{shot};
#endif
        for (std::size_t i = 0; const auto& op : circuit_copy.GetMutOperations()) {
            std::visit(
                [i, &previous_is_measurement, &subflow, &circuit_copy, &state_copy, &op_tasks,
                 &rot_tasks, &postselection_tasks, num_peaks, &result,
                 &simulator](auto&& op_variant) {
                    using OperationType = std::decay_t<decltype(*op_variant)>;
                    using OperationResultType =
                        std::invoke_result_t<OperationType,
                                             State<Float>&>;  // void or Float
                    if constexpr (std::is_same_v<OperationResultType, void>) {
                        // Add operation tasks
                        op_tasks[i].resize(num_peaks);
                        for (std::size_t m = 0; m < num_peaks; ++m) {
                            op_tasks[i][m] = subflow.emplace(
                                [&op_variant, &state_copy, m]() { (*op_variant)(state_copy, m); });
                        }

                        // Set task dependency
                        if (i > 0) {
                            if (previous_is_measurement) {
                                for (std::size_t m = 0; m < num_peaks; ++m) {
                                    op_tasks[i][m].succeed(postselection_tasks.back()[m]);
                                }
                            } else {
                                for (std::size_t m = 0; m < num_peaks; ++m) {
                                    op_tasks[i][m].succeed(op_tasks[i - 1][m]);
                                }
                            }
                        }
                    } else if constexpr (std::is_same_v<OperationType,
                                                        operation::HomodyneSingleMode<Float>>) {
                        const auto mode = op_variant->Modes()[0];

                        // Add preprocess tasks
                        rot_tasks.emplace_back(num_peaks);
                        for (std::size_t m = 0; m < num_peaks; ++m) {
                            rot_tasks.back()[m] =
                                subflow.emplace([&state_copy, m, mode, &op_variant]() {
                                    operation::PhaseRotation<Float>{
                                        mode, -std::numbers::pi_v<Float> / 2 +
                                                  op_variant->GetTheta()}(state_copy, m);
                                });
                        }

                        // Add sampling and FF task
                        op_tasks[i].resize(1);
                        op_tasks[i][0] = subflow.emplace([&op_variant, &state_copy, i,
                                                          &circuit_copy, &result, &simulator,
                                                          mode]() {
                            op_variant->CheckModeIsInState(state_copy);
                            result =
                                operation::HomodyneSingleModeX<Float>{mode,
                                                                      op_variant->GetSelection()}
                                    .Sampling(state_copy, op_variant->GetSelection());
                            const auto shot = state_copy.GetShot();
                            simulator.GetMutMeasuredVal(shot, i) = result.measured_x;
                            circuit_copy.ApplyAllFF(i, simulator.GetMutMeasuredValues(shot), shot);
                        });

                        // Add post-selection tasks
                        postselection_tasks.emplace_back(num_peaks);
                        for (std::size_t m = 0; m < num_peaks; ++m) {
                            postselection_tasks.back()[m] =
                                subflow.emplace([&op_variant, &state_copy, m, &result, mode]() {
                                    operation::HomodyneSingleModeX<Float>{
                                        mode, op_variant->GetSelection()}
                                        .PostSelectBySamplingResult(state_copy, result, m);
                                });
                        }

                        // Set task dependency
                        if (i > 0) {
                            if (previous_is_measurement) {
                                for (std::size_t m = 0; m < num_peaks; ++m) {
                                    rot_tasks.back()[m].succeed(
                                        postselection_tasks[postselection_tasks.size() - 2][m]);
                                    rot_tasks.back()[m].precede(op_tasks[i][0]);
                                    op_tasks[i][0].precede(postselection_tasks.back()[m]);
                                }
                            } else {
                                for (std::size_t m = 0; m < num_peaks; ++m) {
                                    rot_tasks.back()[m].succeed(op_tasks[i - 1][m]);
                                    rot_tasks.back()[m].precede(op_tasks[i][0]);
                                    op_tasks[i][0].precede(postselection_tasks.back()[m]);
                                }
                            }
                        } else {
                            for (std::size_t m = 0; m < num_peaks; ++m) {
                                rot_tasks.back()[m].precede(op_tasks[i][0]);
                                op_tasks[i][0].precede(postselection_tasks.back()[m]);
                            }
                        }
                    } else if constexpr (std::is_same_v<OperationType,
                                                        operation::HomodyneSingleModeX<Float>>) {
                        const auto mode = op_variant->Modes()[0];

                        // Add sampling and FF task
                        op_tasks[i].resize(1);
                        op_tasks[i][0] = subflow.emplace([&op_variant, &state_copy, i,
                                                          &circuit_copy, &result, &simulator,
                                                          mode]() {
                            op_variant->CheckModeIsInState(state_copy);
                            result =
                                operation::HomodyneSingleModeX<Float>{mode,
                                                                      op_variant->GetSelection()}
                                    .Sampling(state_copy, op_variant->GetSelection());
                            const auto shot = state_copy.GetShot();
                            simulator.GetMutMeasuredVal(shot, i) = result.measured_x;
                            circuit_copy.ApplyAllFF(i, simulator.GetMutMeasuredValues(shot), shot);
                        });

                        // Add post-selection tasks
                        postselection_tasks.emplace_back(num_peaks);
                        for (std::size_t m = 0; m < num_peaks; ++m) {
                            postselection_tasks.back()[m] =
                                subflow.emplace([&op_variant, &state_copy, m, &result, mode]() {
                                    operation::HomodyneSingleModeX<Float>{
                                        mode, op_variant->GetSelection().value()}
                                        .PostSelectBySamplingResult(state_copy, result, m);
                                });
                        }

                        // Set task dependency
                        if (i > 0) {
                            if (previous_is_measurement) {
                                for (std::size_t m = 0; m < num_peaks; ++m) {
                                    op_tasks[i][0].succeed(
                                        postselection_tasks[postselection_tasks.size() - 2][m]);
                                    op_tasks[i][0].precede(postselection_tasks.back()[m]);
                                }
                            } else {
                                for (std::size_t m = 0; m < num_peaks; ++m) {
                                    op_tasks[i][0].succeed(op_tasks[i - 1][m]);
                                    op_tasks[i][0].precede(postselection_tasks.back()[m]);
                                }
                            }
                        } else {
                            for (std::size_t m = 0; m < num_peaks; ++m) {
                                op_tasks[i][0].precede(postselection_tasks.back()[m]);
                            }
                        }
                    } else {
                        throw std::runtime_error("Unknown operation");
                    }
                    previous_is_measurement = std::is_same_v<OperationResultType, Float>;
                },
                op);
            ++i;
        }
        subflow.join();
#ifdef BOSIM_BUILD_BENCHMARK
        profile_op.End();
#endif
        if (save_state) { simulator.GetMutShotResult(shot).state = std::move(state_copy); }
    });

    return taskflow;
}

template <std::floating_point Float>
void SimulateCPUMultiThreadShotLevelImpl(const std::uint64_t seed, const std::uint64_t shot,
                                         const Circuit<Float>& circuit, const State<Float>& state,
                                         const bool save_state, Simulator<Float>& simulator) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateCPUMultiThreadShotLevelImpl>{shot};
#endif
    auto state_copy = state;
    state_copy.SetSeed(seed);
    state_copy.SetShot(shot);
    auto circuit_copy = circuit.Copy();
    simulator.Execute(state_copy, circuit_copy);
    if (save_state) { simulator.GetMutShotResult(shot).state = std::move(state_copy); }
}

template <std::floating_point Float>
Result<Float> SimulateCPUMultiThread(const SimulateSettings& settings,
                                     const Circuit<Float>& circuit, const State<Float>& state,
                                     const bool is_shot_level = false) {
#ifdef BOSIM_BUILD_BENCHMARK
    auto profile = profiler::Profiler<profiler::Section::SimulateCPUMultiThread>{};
#endif
    const auto begin = std::chrono::high_resolution_clock::now();
    const auto end_time =
        settings.timeout ? std::make_optional(begin + settings.timeout.value()) : std::nullopt;

    const auto n_shots = settings.n_shots;
    auto simulator = Simulator<Float>{n_shots};

    if (is_shot_level) {
        const auto hardware_concurrency = std::thread::hardware_concurrency();
        const auto max_threads =
            static_cast<std::uint32_t>(hardware_concurrency == 0 ? 1 : hardware_concurrency);

        const auto n_threads = std::min(n_shots, max_threads);
        std::vector<std::thread> threads;
        threads.reserve(n_threads);

        const auto shots_per_thread = n_shots / n_threads;
        const auto n_additional_shots = n_shots % n_threads;

        auto futures = std::vector<std::future<void>>{};
        futures.reserve(n_threads);

        // The first `num_additional_shots` threads execute `shots_per_thread` + 1 shots.
        // The remaining threads execute `shots_per_thread` shots.
        auto shot_start = std::uint32_t{0};
        for (std::uint32_t t = 0; t < n_threads; ++t) {
            const auto num_local_shots =
                static_cast<std::uint32_t>(shots_per_thread + (t < n_additional_shots ? 1 : 0));
            const auto shot_end = shot_start + num_local_shots;

            futures.emplace_back(std::async(std::launch::async, [&end_time, &settings, &circuit,
                                                                 &state, &simulator, shot_start,
                                                                 shot_end]() {
                for (auto shot = shot_start; shot < shot_end; ++shot) {
                    RaiseErrorIfTimedout(end_time);

                    const bool save_state =
                        (settings.save_state_method == StateSaveMethod::All) ||
                        (settings.save_state_method == StateSaveMethod::FirstOnly && shot == 0u);
                    SimulateCPUMultiThreadShotLevelImpl(settings.seed + shot, shot, circuit, state,
                                                        save_state, simulator);
                }
            }));
            shot_start = shot_end;
        }

        for (auto& future : futures) { future.get(); }
    } else {
        auto taskflow = tf::Taskflow{};
        auto shot_tasks = std::vector<tf::Task>{};
        shot_tasks.reserve(n_shots);
        auto shot_tfs = std::vector<tf::Taskflow>{};
        shot_tfs.reserve(n_shots);

        for (auto i = 0u; i < n_shots; ++i) {
            const bool save_state =
                (settings.save_state_method == StateSaveMethod::All) ||
                (settings.save_state_method == StateSaveMethod::FirstOnly && i == 0u);
            shot_tfs.push_back(SimulateCPUMultiThreadImpl(end_time, settings.seed + i, i, circuit,
                                                          state, save_state, simulator));
            shot_tasks.push_back(taskflow.composed_of(shot_tfs.back()));
        }

        auto executor = tf::Executor{};
        executor.run(taskflow).get();
    }

    const auto end = std::chrono::high_resolution_clock::now();
    simulator.GetMutResult().elapsed_ms = std::chrono::duration_cast<Ms>(end - begin);
    return simulator.GetResult();
}
}  // namespace bosim
