#pragma once

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace bosim {
using Ms = std::chrono::duration<double, std::milli>;

#ifdef BOSIM_BUILD_BENCHMARK
namespace profiler {
enum class Section : std::uint8_t {
    None,  // DO NOT REMOVE: the first item must be none

    SimulateCPUSingleThread,
    SimulateCPUSingleThreadImpl,
    SimulateCPUMultiThread,
    SimulateCPUMultiThreadImpl,
    SimulateCPUMultiThreadShotLevelImpl,
#ifdef BOSIM_USE_GPU
    SimulateSingleGPU,
    SimulateMultiGPU,
    SimulateSingleGPUImpl,
    UploadStateToGPU,
    DownloadStateToCPU,
    CopyStateGPU,
#endif
    ShotLevelTaskflow,
    ShotLevelTaskflowOperationLoop,

    Execute,
    ExecuteOperation,
    ApplyAllFF,

    UnaryOperation,
    UnaryOperationPerPeak,
    BinaryOperation,
    BinaryOperationPerPeak,
    HomodyneSingleModeX,
    HomodyneSingleMode,
    Sampling,
    PostSelectBySamplingResult,
    PostSelectBySamplingResultPerPeak,
    PostSelectByMeasuredValue,

    ConstructState,

    InitializeGraphModes,
    ConvertGraphToCircuit,
    ExtractGraphResult,

    InitializeMachineryModes,
    ConvertMachineryToCircuit,
    ExtractMachineryResult,

    Count,  // DO NOT REMOVE: the last item must be counter of items
};

static inline const char* SectionToString(const Section s) {
    switch (s) {
#define PROFILER_TOOL_SECTION_TOSTRING_MAIN(S) \
    case Section::S: return #S
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateCPUSingleThread);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateCPUSingleThreadImpl);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateCPUMultiThread);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateCPUMultiThreadImpl);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateCPUMultiThreadShotLevelImpl);
#ifdef BOSIM_USE_GPU
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateSingleGPU);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateMultiGPU);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(SimulateSingleGPUImpl);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(UploadStateToGPU);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(DownloadStateToCPU);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(CopyStateGPU);
#endif
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ShotLevelTaskflow);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ShotLevelTaskflowOperationLoop);

        PROFILER_TOOL_SECTION_TOSTRING_MAIN(Execute);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ExecuteOperation);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ApplyAllFF);

        PROFILER_TOOL_SECTION_TOSTRING_MAIN(UnaryOperation);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(UnaryOperationPerPeak);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(BinaryOperation);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(BinaryOperationPerPeak);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(HomodyneSingleModeX);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(HomodyneSingleMode);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(Sampling);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(PostSelectBySamplingResult);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(PostSelectBySamplingResultPerPeak);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(PostSelectByMeasuredValue);

        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ConstructState);

        PROFILER_TOOL_SECTION_TOSTRING_MAIN(InitializeGraphModes);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ConvertGraphToCircuit);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ExtractGraphResult);

        PROFILER_TOOL_SECTION_TOSTRING_MAIN(InitializeMachineryModes);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ConvertMachineryToCircuit);
        PROFILER_TOOL_SECTION_TOSTRING_MAIN(ExtractMachineryResult);

#undef PROFILER_TOOL_SECTION_TOSTRING_MAIN

        case Section::None: [[fallthrough]];
        case Section::Count: return "";
        default: throw std::logic_error("unreachable");
    }
    throw std::runtime_error("Invalid section");
}

class Entry final {
public:
    enum class StateType : std::uint8_t { Begin, End };

    using TimePointType = std::chrono::time_point<std::chrono::high_resolution_clock>;

    Section section;
    StateType state;
    TimePointType time_point;

    Entry(decltype(section) n, decltype(state) s, decltype(time_point) t)
        : section(n), state(s), time_point(t) {}

    Entry(decltype(section) n, decltype(state) s)
        : Entry(n, s, std::chrono::high_resolution_clock::now()) {}
};

class Entries final {
public:
    template <Section F>
    void Begin() {
        data_.emplace_back(F, Entry::StateType::Begin);
    }

    template <Section F>
    void End() {
        data_.emplace_back(F, Entry::StateType::End);
    }

    void Sort() {
        std::sort(data_.begin(), data_.end(), [](const Entry& left, const Entry& right) {
            return left.time_point < right.time_point;
        });
    }

    void Clear() { data_.clear(); }

    const std::vector<Entry>& Data() const { return data_; }

private:
    std::vector<Entry> data_;
};

/**
 * @brief Singleton for time recording.
 *
 * @details It is a thread-safe container that supports shot-level parallelization and peak-level
 * parallelization in simulations. Before recording time, the number of shots and peaks must be
 * specified using the 'Resize' method.
 */
class TimeRecord {
public:
    const Entry::TimePointType origin;

    static TimeRecord& GetInstance() {  // Keep the linkage global. This is "singleton".
        static TimeRecord Instance;
        return Instance;
    }

    Entries& GetExecutionRecord() { return execution_record_; }
    std::vector<Entries>& GetShotRecord() { return shot_record_; }
    std::vector<std::vector<Entries>>& GetPeakRecord() { return peak_record_; }

    void Resize(const std::size_t n_shots, const std::size_t n_peaks) {
        shot_record_.resize(n_shots);
        peak_record_.resize(n_shots);
        for (auto&& peak_rec : peak_record_) { peak_rec.resize(n_peaks); }
    }

    void Clear() {
        execution_record_.Clear();
        shot_record_.clear();
        peak_record_.clear();
    }

    void Enable() { enabled_ = true; }
    void Disable() { enabled_ = false; }
    bool IsEnabled() const { return enabled_; }

private:
    TimeRecord() : origin(std::chrono::high_resolution_clock::now()) {}

    Entries execution_record_;
    std::vector<Entries> shot_record_;
    std::vector<std::vector<Entries>> peak_record_;
    bool enabled_ = false;
};

/**
 * @brief Tool to add recorded time to the 'TimeRecord' singleton.
 *
 * @tparam S Section name identifying which part of the code execution time to measure.
 *
 * @details The time when an instance is created and the earlier of either its expiration or the End
 * method call is recorded in the 'TimeRecord' singleton. Recording only occurs if the 'Enable'
 * method of 'TimeRecord' has been called beforehand to enable it. Additionally, before recording
 * time, the number of shots and peaks must be specified using the 'Resize' method of 'TimeRecord'.
 */
template <Section S>
class Profiler final {
    static_assert(S != Section::Count, "Do not use Count");

public:
    Profiler() {
        if (TimeRecord::GetInstance().IsEnabled()) {
            entries_ = &(TimeRecord::GetInstance().GetExecutionRecord());
        }
        Begin();
    }
    explicit Profiler(const std::size_t shot) {
        if (TimeRecord::GetInstance().IsEnabled()) {
            entries_ = &(TimeRecord::GetInstance().GetShotRecord().at(shot));
        }
        Begin();
    }
    Profiler(const std::size_t shot, const std::size_t peak) {
        if (TimeRecord::GetInstance().IsEnabled()) {
            entries_ = &(TimeRecord::GetInstance().GetPeakRecord().at(shot).at(peak));
        }
        Begin();
    }

    ~Profiler() { End(); }

    void End() {
        if (TimeRecord::GetInstance().IsEnabled() && valid_) {
            entries_->End<S>();
            valid_ = false;
        }
    }

private:
    bool valid_ = false;
    Entries* entries_;

    void Begin() {
        if (TimeRecord::GetInstance().IsEnabled() && !valid_) {
            entries_->Begin<S>();
            valid_ = true;
        }
    }
};

class ProfilerTool {
public:
    using TimeType = std::chrono::nanoseconds;

    static const auto& Logs() {
        auto& record = TimeRecord::GetInstance().GetExecutionRecord();
        record.Sort();
        return record.Data();
    }
    static const auto& Logs(const std::size_t shot) {
        auto& record = TimeRecord::GetInstance().GetShotRecord()[shot];
        record.Sort();
        return record.Data();
    }
    static const auto& Logs(const std::size_t shot, const std::size_t peak) {
        auto& record = TimeRecord::GetInstance().GetPeakRecord()[shot][peak];
        record.Sort();
        return record.Data();
    }

    static auto SectionName(const Section s) { return std::string{SectionToString(s)}; }

    static auto State(const Entry::StateType s) {
        switch (s) {
            case Entry::StateType::Begin: return "begin";
            case Entry::StateType::End: return "end";
            default: throw std::logic_error("unreachable");
        }
        throw std::runtime_error("Invalid state");
    }

    static auto Time(const Entry::TimePointType t) {
        return std::chrono::duration_cast<TimeType>(t - TimeRecord::GetInstance().origin).count();
    }

    static auto Durations() -> std::unordered_map<std::string, std::vector<long>> {
        auto time_points = std::unordered_map<std::string, std::vector<long>>{};

        auto& execution_record = TimeRecord::GetInstance().GetExecutionRecord();
        if (execution_record.Data().size() % 2 != 0) {
            throw std::runtime_error("Record length must be even");
        }
        execution_record.Sort();
        for (const auto& entry : execution_record.Data()) {
            time_points[SectionName(entry.section)].push_back(Time(entry.time_point));
        }

        auto& shot_record = TimeRecord::GetInstance().GetShotRecord();
        for (auto&& record : shot_record) {
            if (record.Data().size() % 2 != 0) {
                throw std::runtime_error("Record length must be even");
            }
            record.Sort();
            for (const auto& entry : record.Data()) {
                time_points[SectionName(entry.section)].push_back(Time(entry.time_point));
            }
        }

        auto& peak_record = TimeRecord::GetInstance().GetPeakRecord();
        for (auto&& shot_record : peak_record) {
            for (auto&& record : shot_record) {
                if (record.Data().size() % 2 != 0) {
                    throw std::runtime_error("Record length must be even");
                }
                record.Sort();
                for (const auto& entry : record.Data()) {
                    time_points[SectionName(entry.section)].push_back(Time(entry.time_point));
                }
            }
        }

        auto durations = std::unordered_map<std::string, std::vector<long>>{};
        for (const auto& [name, v_time] : time_points) {
            const auto size = v_time.size();
            for (std::size_t i = 0; i < size; ++ ++i) {
                durations[name].push_back(v_time[i + 1] - v_time[i]);
            }
        }
        return durations;
    }

    static std::unordered_map<std::string, double> Medians(
        const std::unordered_map<std::string, std::vector<long>>& durations = Durations()) {
        auto medians = std::unordered_map<std::string, double>{};
        for (const auto& [name, v] : durations) {
            auto v_cp = v;
            std::sort(v_cp.begin(), v_cp.end());
            const std::size_t median_idx = v_cp.size() / 2;
            medians[name] = v_cp.size() % 2 == 0
                                ? static_cast<double>(v_cp[median_idx] + v_cp[median_idx - 1]) / 2
                                : static_cast<double>(v_cp[median_idx]);
        }
        return medians;
    }

    static void PrintSectionNamesImpl(std::ostream& os) {
        for (std::underlying_type_t<Section> i = 1;
             i < static_cast<std::underlying_type_t<Section>>(Section::Count); ++i) {
            auto section = static_cast<Section>(i);
            os << ProfilerTool::SectionName(section) << ",";
        }
        os << "\n";
    }

    static void PrintSectionNames() { PrintSectionNamesImpl(std::cout); }
    static void SaveSectionNames(const std::filesystem::path& file_path) {
        auto ofs = std::ofstream{file_path, std::ios::app};
        if (!ofs) {
            throw std::runtime_error(fmt::format("Failed to open output file '{}' for appending.\n",
                                                 file_path.string()));
        }
        PrintSectionNamesImpl(ofs);
        ofs.flush();
        ofs.close();
    }

    static void PrintMediansMsImpl(
        std::ostream& os, const std::unordered_map<std::string, double>& medians = Medians()) {
        for (std::underlying_type_t<Section> i = 1;
             i < static_cast<std::underlying_type_t<Section>>(Section::Count); ++i) {
            auto section = static_cast<Section>(i);
            if (auto it = medians.find(ProfilerTool::SectionName(section)); it != medians.end()) {
                os << it->second * 1e-6;
            }
            os << ",";
        }
        os << "\n";
    }

    static void PrintMediansMs(const std::unordered_map<std::string, double>& medians = Medians()) {
        PrintMediansMsImpl(std::cout, medians);
    }
    static void SaveMediansMs(const std::filesystem::path& file_path,
                              const std::unordered_map<std::string, double>& medians = Medians()) {
        auto ofs = std::ofstream{file_path, std::ios::app};
        if (!ofs) {
            std::cerr << fmt::format("Error: Failed to open output file '{}' for appending.\n",
                                     file_path.string());
        }
        PrintMediansMsImpl(ofs, medians);
        ofs.flush();
        ofs.close();
    }
};
}  // namespace profiler
#endif

}  // namespace bosim
