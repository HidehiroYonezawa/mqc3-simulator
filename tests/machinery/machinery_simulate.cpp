#include "bosim/simulate/machinery_simulate.h"

#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "bosim/simulate/machinery_repr.h"

#include "machinery_repr.h"

std::tuple<double, double, double, double> ConvertedMeasuredValues(const double m_a,
                                                                   const double m_b,
                                                                   const double m_c,
                                                                   const double m_d) {
    return {(m_a - m_b + m_c - m_d) / 2, (m_a + m_b + m_c + m_d) / 2, (-m_a + m_b + m_c - m_d) / 2,
            (-m_a - m_b + m_c + m_d) / 2};
}

TEST(Machinery, ExtractMachineryResult) {
    auto shot_result_0 = bosim::ShotResult<double>{};
    auto shot_result_1 = bosim::ShotResult<double>{};
    for (std::size_t i = 0; i < 100; ++i) {
        shot_result_0.measured_values[i] = static_cast<double>(i);
        shot_result_1.measured_values[i] = static_cast<double>(i * 2);
    }
    auto result = bosim::Result<double>{};
    result.result = {shot_result_0, shot_result_1};

    auto machinery_repr = bosim::machinery::PhysicalCircuitConfiguration<double>{};
    constexpr std::size_t FirstMacronodeIndex = 0;
    constexpr std::size_t SecondMacronodeIndex = 3;
    machinery_repr.readout_macronodes_indices = {FirstMacronodeIndex, SecondMacronodeIndex};

    auto pb_machinery_result =
        bosim::machinery::ExtractMachineryResult<double>(result, machinery_repr);

    const auto& pb_smv_0 = pb_machinery_result.measured_vals().at(0);
    {
        const auto& pb_mmv_0_0 = pb_smv_0.measured_vals().at(0);
        auto [m_converted_a, m_converted_b, m_converted_c, m_converted_d] =
            ConvertedMeasuredValues(18 * FirstMacronodeIndex + 7, 18 * FirstMacronodeIndex + 8,
                                    18 * FirstMacronodeIndex + 9, 18 * FirstMacronodeIndex + 10);
        EXPECT_DOUBLE_EQ(pb_mmv_0_0.m_a(), m_converted_a);
        EXPECT_DOUBLE_EQ(pb_mmv_0_0.m_b(), m_converted_b);
        EXPECT_DOUBLE_EQ(pb_mmv_0_0.m_c(), m_converted_c);
        EXPECT_DOUBLE_EQ(pb_mmv_0_0.m_d(), m_converted_d);
    }
    {
        const auto& pb_mmv_0_1 = pb_smv_0.measured_vals().at(1);
        auto [m_converted_a, m_converted_b, m_converted_c, m_converted_d] =
            ConvertedMeasuredValues(18 * SecondMacronodeIndex + 7, 18 * SecondMacronodeIndex + 8,
                                    18 * SecondMacronodeIndex + 9, 18 * SecondMacronodeIndex + 10);
        EXPECT_DOUBLE_EQ(pb_mmv_0_1.m_a(), m_converted_a);
        EXPECT_DOUBLE_EQ(pb_mmv_0_1.m_b(), m_converted_b);
        EXPECT_DOUBLE_EQ(pb_mmv_0_1.m_c(), m_converted_c);
        EXPECT_DOUBLE_EQ(pb_mmv_0_1.m_d(), m_converted_d);
    }
    const auto& pb_smv_1 = pb_machinery_result.measured_vals().at(1);
    {
        auto [m_converted_a, m_converted_b, m_converted_c, m_converted_d] = ConvertedMeasuredValues(
            (18 * FirstMacronodeIndex + 7) * 2, (18 * FirstMacronodeIndex + 8) * 2,
            (18 * FirstMacronodeIndex + 9) * 2, (18 * FirstMacronodeIndex + 10) * 2);
        const auto& pb_mmv_1_0 = pb_smv_1.measured_vals().at(0);
        EXPECT_DOUBLE_EQ(pb_mmv_1_0.m_a(), m_converted_a);
        EXPECT_DOUBLE_EQ(pb_mmv_1_0.m_b(), m_converted_b);
        EXPECT_DOUBLE_EQ(pb_mmv_1_0.m_c(), m_converted_c);
        EXPECT_DOUBLE_EQ(pb_mmv_1_0.m_d(), m_converted_d);
    }
    {
        auto [m_converted_a, m_converted_b, m_converted_c, m_converted_d] = ConvertedMeasuredValues(
            (18 * SecondMacronodeIndex + 7) * 2, (18 * SecondMacronodeIndex + 8) * 2,
            (18 * SecondMacronodeIndex + 9) * 2, (18 * SecondMacronodeIndex + 10) * 2);
        const auto& pb_mmv_1_1 = pb_smv_1.measured_vals().at(1);
        EXPECT_DOUBLE_EQ(pb_mmv_1_1.m_a(), m_converted_a);
        EXPECT_DOUBLE_EQ(pb_mmv_1_1.m_b(), m_converted_b);
        EXPECT_DOUBLE_EQ(pb_mmv_1_1.m_c(), m_converted_c);
        EXPECT_DOUBLE_EQ(pb_mmv_1_1.m_d(), m_converted_d);
    }
}

TEST(Machinery, MachinerySimulate) {
    const auto env = bosim::python::PythonEnvironment();
    auto machinery_repr = TestConfig();

    constexpr std::size_t NShots = 100;
    constexpr double SqueezingLevel = 10;
    const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::CPUSingleThread,
                                                  bosim::StateSaveMethod::None};

    auto pb_result =
        bosim::machinery::MachinerySimulate<double>(settings, machinery_repr, SqueezingLevel);
}

TEST(Machinery, NLFF) {
    const auto env = bosim::python::PythonEnvironment();
    auto machinery_repr = TestConfig();

    constexpr std::size_t NShots = 1;
    constexpr double SqueezingLevel = 10;
    const auto settings = bosim::SimulateSettings{NShots, bosim::Backend::CPUSingleThread,
                                                  bosim::StateSaveMethod::None};
    // NOTE: If the backend is set as multi-threaded, the following tests fail as if NLFF is not
    // applied. This is because a copy of the circuit object is used in the simulation instead of
    // the original circuit object.

    const auto [circuit, result] =
        bosim::machinery::MachinerySimulateImpl<double>(settings, machinery_repr, SqueezingLevel);
    const auto machinery_result = bosim::machinery::ExtractMachineryResult(result, machinery_repr);

    {  // NLFF 0
        constexpr auto SourceMacronodeIdx = 0;
        const auto source_measured_val =
            machinery_result.measured_vals()[0].measured_vals()[3 - SourceMacronodeIdx].m_a();

        constexpr auto TargetMacronodeIdx = 2;
        constexpr auto TargetMacronodeOpIdx =
            static_cast<std::size_t>(bosim::machinery::MacronodeOpType::DisplacementD);
        const auto& op_ptr = circuit.GetOpPtr<bosim::operation::Displacement<double>>(
            bosim::machinery::NOpsPerMacronode * TargetMacronodeIdx + TargetMacronodeOpIdx);

        EXPECT_FLOAT_EQ(op_ptr->GetX(), 2 * source_measured_val);
    }
    {  // NLFF 1
        constexpr auto SourceMacronodeIdx = 0;
        const auto source_measured_val =
            machinery_result.measured_vals()[0].measured_vals()[3 - SourceMacronodeIdx].m_a();

        constexpr auto TargetMacronodeIdx = 2;
        constexpr auto TargetMacronodeOpIdx =
            static_cast<std::size_t>(bosim::machinery::MacronodeOpType::DisplacementB);
        const auto& op_ptr = circuit.GetOpPtr<bosim::operation::Displacement<double>>(
            bosim::machinery::NOpsPerMacronode * TargetMacronodeIdx + TargetMacronodeOpIdx);

        EXPECT_FLOAT_EQ(op_ptr->GetP(), source_measured_val * source_measured_val);
    }
    {  // NLFF 2
        constexpr auto SourceMacronodeIdx = 2;
        const auto source_measured_val =
            machinery_result.measured_vals()[0].measured_vals()[3 - SourceMacronodeIdx].m_b();

        constexpr auto TargetMacronodeIdx = 3;
        constexpr auto TargetMacronodeOpIdx =
            static_cast<std::size_t>(bosim::machinery::MacronodeOpType::DisplacementB);
        const auto& op_ptr = circuit.GetOpPtr<bosim::operation::Displacement<double>>(
            bosim::machinery::NOpsPerMacronode * TargetMacronodeIdx + TargetMacronodeOpIdx);

        EXPECT_FLOAT_EQ(op_ptr->GetX(),
                        source_measured_val * source_measured_val * source_measured_val);
    }
    {  // NLFF 3
        constexpr auto SourceMacronodeIdx = 1;
        const auto source_measured_val =
            machinery_result.measured_vals()[0].measured_vals()[3 - SourceMacronodeIdx].m_d();

        constexpr auto TargetMacronodeIdx = 2;
        constexpr auto TargetMacronodeOpIdx =
            static_cast<std::size_t>(bosim::machinery::MacronodeOpType::HomodyneA);
        const auto& op_ptr = circuit.GetOpPtr<bosim::operation::HomodyneSingleMode<double>>(
            bosim::machinery::NOpsPerMacronode * TargetMacronodeIdx + TargetMacronodeOpIdx);

        EXPECT_FLOAT_EQ(op_ptr->GetTheta(), 2 * source_measured_val + 1);
    }
}
