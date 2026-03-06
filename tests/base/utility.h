#pragma once

#include <fmt/format.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <concepts>
#include <filesystem>  // NOLINT
#include <fstream>
#include <string>

#include "bosim/state.h"

template <std::floating_point Float>
bool CompareFloat(const Float val1, const Float val2, const Float EPS = 1e-6) {
    return std::abs(val1 - val2) < EPS;
}

template <std::floating_point Float>
bool CompareState(const bosim::State<Float>& state1, const bosim::State<Float>& state2) {
    if (state1.NumModes() != state2.NumModes()) return false;
    if (state1.NumPeaks() != state2.NumPeaks()) return false;

    for (std::size_t i = 0; i < state1.NumPeaks(); ++i) {
        // Coeff
        const auto& coeff1 = state1.GetCoeff(i);
        const auto& coeff2 = state2.GetCoeff(i);
        if (!CompareFloat(coeff1.real(), coeff2.real())) return false;
        if (!CompareFloat(coeff1.imag(), coeff2.imag())) return false;

        // Mean
        for (long j = 0; j < state1.GetMean(i).size(); ++j) {
            const auto& mean1 = state1.GetMean(i)[j];
            const auto& mean2 = state2.GetMean(i)[j];

            if (!CompareFloat(mean1.real(), mean2.real())) return false;
            if (!CompareFloat(mean1.imag(), mean2.imag())) return false;
        }

        // Cov
        for (long j = 0; j < state1.GetCov(i).rows(); ++j) {
            for (long k = 0; k < state1.GetCov(i).cols(); ++k) {
                const auto& cov1 = state1.GetCov(i)(j, k);
                const auto& cov2 = state2.GetCov(i)(j, k);

                if (!CompareFloat(cov1, cov2)) return false;
            }
        }
    }
    return true;
}

template <class... Args>
    requires(std::convertible_to<Args, std::string> && ...)
void TestInPython(const std::string& script_name, const std::string& func_name, Args&&... args) {
    const int code_find_py = std::system("python --version");  // NOLINT(cert-env33-c)
    ASSERT_EQ(code_find_py, 0) << "Python script failed with exit code " << code_find_py;

    const auto test_dir = std::filesystem::path{__FILE__}.parent_path();
    const auto py_script = test_dir / fmt::format("{}.py", script_name);
    auto command = fmt::format("python \"{}\" {}", py_script.string(), func_name);

    ((command += fmt::format(" {}", std::forward<Args>(args))), ...);

    std::cout << "Executing Python: " << command << std::endl;
    const int code = std::system(command.c_str());  // NOLINT(cert-env33-c)
    EXPECT_EQ(code, 0) << "Python script failed with exit code " << code;
}

inline void AddToJson(const std::string& file, const std::map<std::string, nlohmann::json>& items,
                      const bool new_file = false) {
    auto json = nlohmann::json{};
    if (!new_file) {
        if (std::ifstream ifs{file}) ifs >> json;
    }
    for (const auto& [key, value] : items) { json[key] = value; }
    auto ofs = std::ofstream{file};
    ofs << json.dump(4);
}

template <std::floating_point Float>
void AddWignerToJson(const std::string& file, const bosim::State<Float>& state,
                     const std::initializer_list<std::size_t> modes, const double start = -15.0,
                     const double stop = 15.0, const std::size_t num = 201,
                     const bool new_file = false) {
    auto json = nlohmann::json{};
    if (!new_file) {
        if (std::ifstream ifs{file}) ifs >> json;
    }
    auto wigner = nlohmann::json{};
    wigner["start"] = start;
    wigner["stop"] = stop;
    wigner["num"] = num;

    const double dx = (stop - start) / static_cast<double>(num - 1);
    auto xvec = std::vector<double>(num);
    for (std::size_t i = 0; i < num; ++i) { xvec[i] = start + static_cast<double>(i) * dx; }

    auto mode_val_map = nlohmann::json{};
    for (const auto mode : modes) {
        auto vals = nlohmann::ordered_json::array();
        for (std::size_t i = 0; i < num; ++i) {
            for (std::size_t j = 0; j < num; ++j) {
                vals.push_back(state.WignerFuncValue(mode, xvec[j], xvec[i]).real());
            }
        }
        mode_val_map[std::to_string(mode)] = vals;
    }
    wigner["vals"] = mode_val_map;

    json["wigner"] = wigner;
    auto ofs = std::ofstream{file};
    ofs << json.dump(4);
}
