#pragma once

#include "bosim/simulate/machinery_repr.h"

inline bosim::machinery::PhysicalCircuitConfiguration<double> TestConfig() {
    auto config = bosim::machinery::PhysicalCircuitConfiguration<double>{};
    config.n_local_macronodes = 2;
    config.n_steps = 3;
    config.homodyne_angles = {
        bosim::machinery::MacronodeAngle<double>{0.1, 0.1, 0.1, 0.1},  // Initialization
        bosim::machinery::MacronodeAngle<double>{0.1, 0.2, 0.3, 0.4},
        bosim::machinery::MacronodeAngle<double>{0.5, 0.6, 0.7, 0.8},
        bosim::machinery::MacronodeAngle<double>{0.2, 0.2, 0.2, 0.2},  // Measurement
        bosim::machinery::MacronodeAngle<double>{0.9, 1.0, 1.1, 1.2},
        bosim::machinery::MacronodeAngle<double>{1.3, 1.4, 1.5, 1.6}};
    config.generating_method_for_ff_coeff_k_plus_1 = {
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_FROM_HOMODYNE_ANGLES,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_FROM_HOMODYNE_ANGLES,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_FROM_HOMODYNE_ANGLES,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED};
    config.generating_method_for_ff_coeff_k_plus_n = {
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_FROM_HOMODYNE_ANGLES,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED,
        bosim::machinery::PbFeedForwardCoefficientGenerationMethod::
            FEED_FORWARD_COEFFICIENT_GENERATION_METHOD_ZERO_FILLED};
    config.displacements_k_minus_1 = {bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.1, 0.2},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0}};
    config.displacements_k_minus_n = {bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.3, 0.4},
                                      bosim::machinery::DisplacementComplex<double>{0.5, 0.6},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0},
                                      bosim::machinery::DisplacementComplex<double>{0.0, 0.0}};
    config.readout_macronodes_indices = {3, 2, 1, 0};  // intentionally reversed just for tests

    auto nlff0 = bosim::machinery::MachineryFF{};
    nlff0.function = 2;
    nlff0.from_macronode = 0;
    nlff0.from_abcd = 0;
    nlff0.to_macronode = 2;
    nlff0.to_parameter = 6;  // X of displacement_k_minus_n
    auto nlff1 = bosim::machinery::MachineryFF{};
    nlff1.function = 0;
    nlff1.from_macronode = 0;
    nlff1.from_abcd = 0;
    nlff1.to_macronode = 2;
    nlff1.to_parameter = 5;  // P of displacement_k_minus_1
    auto nlff2 = bosim::machinery::MachineryFF{};
    nlff2.function = 1;
    nlff2.from_macronode = 2;
    nlff2.from_abcd = 1;
    nlff2.to_macronode = 3;
    nlff2.to_parameter = 4;  // X of displacement_k_minus_1
    auto nlff3 = bosim::machinery::MachineryFF{};
    nlff3.function = 3;
    nlff3.from_macronode = 1;
    nlff3.from_abcd = 3;
    nlff3.to_macronode = 2;
    nlff3.to_parameter = 0;  // theta_a
    config.nlffs = std::vector<bosim::machinery::MachineryFF>{nlff0, nlff1, nlff2, nlff3};

    constexpr auto FuncStr0 = R"(def f0(x):
        return x * x)";
    constexpr auto FuncStr1 = R"(def f1(x):
        return x * x * x)";
    constexpr auto FuncStr2 = R"(def f2(x):
        return 2 * x)";
    constexpr auto FuncStr3 = R"(def f3(x):
        return 2 * x + 1)";
    auto function0 = bosim::python::PythonFeedForward({FuncStr0});
    auto function1 = bosim::python::PythonFeedForward({FuncStr1});
    auto function2 = bosim::python::PythonFeedForward({FuncStr2});
    auto function3 = bosim::python::PythonFeedForward({FuncStr3});
    config.functions =
        std::vector<bosim::python::PythonFeedForward>{function0, function1, function2, function3};

    return config;
}
