#include "bosim/simulate/machinery_repr.h"

#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include "machinery_repr.h"

#ifndef NDEBUG
TEST(Machinery, MacronodeAngle) {
    constexpr auto Pi = std::numbers::pi_v<double>;
    EXPECT_THROW(bosim::machinery::MacronodeAngle(1.0, 1.0, 2.0, 2.0), std::invalid_argument);
    EXPECT_THROW(bosim::machinery::MacronodeAngle(2.0, 2.0, 1.0, 1.0), std::invalid_argument);
    EXPECT_THROW(bosim::machinery::MacronodeAngle(1.0, 1.0, Pi, 3 * Pi), std::invalid_argument);
    EXPECT_THROW(bosim::machinery::MacronodeAngle(1.0, 1.0, 2 * Pi, Pi), std::invalid_argument);
    EXPECT_THROW(bosim::machinery::MacronodeAngle(Pi, 2 * Pi, 1.0, 1.0), std::invalid_argument);
    EXPECT_THROW(bosim::machinery::MacronodeAngle(3 * Pi, Pi, 1.0, 1.0), std::invalid_argument);
}

TEST(Machinery, FMacIMacError) {
    constexpr auto Pi = std::numbers::pi_v<double>;
    EXPECT_THROW(bosim::machinery::FMac<double>(
                     bosim::machinery::MacronodeAngle{1.0, 1.0, 1.0, 1.0},
                     bosim::machinery::MacronodeMeasuredValue{1.0, 2.0, 3.0, 4.0}),
                 std::invalid_argument);
    EXPECT_THROW(bosim::machinery::FMac<double>(
                     bosim::machinery::MacronodeAngle{Pi, 2 * Pi, 3 * Pi, 4 * Pi},
                     bosim::machinery::MacronodeMeasuredValue{1.0, 2.0, 3.0, 4.0}),
                 std::invalid_argument);
    EXPECT_THROW(bosim::machinery::IMac<double>(
                     bosim::machinery::MacronodeAngle{1.0, 2.0, 3.0, 4.0},
                     bosim::machinery::MacronodeMeasuredValue{1.0, 2.0, 3.0, 4.0}),
                 std::invalid_argument);
}
#endif

TEST(Machinery, FMac) {
    constexpr double ThetaA = 0.1;
    constexpr double ThetaB = 0.2;
    constexpr double ThetaC = 0.3;
    constexpr double ThetaD = 0.4;
    constexpr double MeasuredA = 0.5;
    constexpr double MeasuredB = 0.6;
    constexpr double MeasuredC = 0.7;
    constexpr double MeasuredD = 0.8;
    const auto x1 =
        -(MeasuredA * std::cos(ThetaB) + MeasuredB * std::cos(ThetaA)) / std::sin(ThetaA - ThetaB) -
        (MeasuredC * std::cos(ThetaD) + MeasuredD * std::cos(ThetaC)) / std::sin(ThetaC - ThetaD);
    const auto p1 =
        -(MeasuredA * std::sin(ThetaB) + MeasuredB * std::sin(ThetaA)) / std::sin(ThetaA - ThetaB) -
        (MeasuredC * std::sin(ThetaD) + MeasuredD * std::sin(ThetaC)) / std::sin(ThetaC - ThetaD);
    const auto xn =
        (MeasuredA * std::cos(ThetaB) + MeasuredB * std::cos(ThetaA)) / std::sin(ThetaA - ThetaB) -
        (MeasuredC * std::cos(ThetaD) + MeasuredD * std::cos(ThetaC)) / std::sin(ThetaC - ThetaD);
    const auto pn =
        (MeasuredA * std::sin(ThetaB) + MeasuredB * std::sin(ThetaA)) / std::sin(ThetaA - ThetaB) -
        (MeasuredC * std::sin(ThetaD) + MeasuredD * std::sin(ThetaC)) / std::sin(ThetaC - ThetaD);
    const auto [d1_actual, dn_actual] =
        bosim::machinery::FMac(bosim::machinery::MacronodeAngle{0.1, 0.2, 0.3, 0.4},
                               bosim::machinery::MacronodeMeasuredValue{0.5, 0.6, 0.7, 0.8});
    EXPECT_DOUBLE_EQ(x1, d1_actual.x);
    EXPECT_DOUBLE_EQ(p1, d1_actual.p);
    EXPECT_DOUBLE_EQ(xn, dn_actual.x);
    EXPECT_DOUBLE_EQ(pn, dn_actual.p);
}

TEST(Machinery, MachineryCircuitHomodyne) {
    const auto env = bosim::python::PythonEnvironment();

    auto config = TestConfig();
    auto circuit = MachineryCircuit<double>(config, 10);
    const auto& ops = circuit.GetOperations();
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 0);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.1);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(0, bosim::machinery::MacronodeOpType::HomodyneB)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 1);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.1);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(0, bosim::machinery::MacronodeOpType::HomodyneD)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 2);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.1);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(0, bosim::machinery::MacronodeOpType::HomodyneC)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 3);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.1);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(0, bosim::machinery::MacronodeOpType::HomodyneA)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 4);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.2);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(1, bosim::machinery::MacronodeOpType::HomodyneB)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 5);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.4);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(1, bosim::machinery::MacronodeOpType::HomodyneD)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 0);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.3);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(1, bosim::machinery::MacronodeOpType::HomodyneC)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 1);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.1);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(1, bosim::machinery::MacronodeOpType::HomodyneA)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 2);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.6);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(2, bosim::machinery::MacronodeOpType::HomodyneB)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 6);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.8);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(2, bosim::machinery::MacronodeOpType::HomodyneD)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 4);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.7);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(2, bosim::machinery::MacronodeOpType::HomodyneC)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 5);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.5);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(2, bosim::machinery::MacronodeOpType::HomodyneA)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 0);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.2);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(3, bosim::machinery::MacronodeOpType::HomodyneB)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 3);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.2);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(3, bosim::machinery::MacronodeOpType::HomodyneD)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 2);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.2);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(3, bosim::machinery::MacronodeOpType::HomodyneC)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 6);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.2);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(3, bosim::machinery::MacronodeOpType::HomodyneA)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 4);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.0);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(4, bosim::machinery::MacronodeOpType::HomodyneB)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 1);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.2);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(4, bosim::machinery::MacronodeOpType::HomodyneD)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 0);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.1);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(4, bosim::machinery::MacronodeOpType::HomodyneC)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 3);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 0.9);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(4, bosim::machinery::MacronodeOpType::HomodyneA)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 2);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.4);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(5, bosim::machinery::MacronodeOpType::HomodyneB)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 5);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.6);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(5, bosim::machinery::MacronodeOpType::HomodyneD)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 4);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.5);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(5, bosim::machinery::MacronodeOpType::HomodyneC)]);
    std::visit(
        [](auto&& op_variant) {
            using OperationType = std::decay_t<decltype(*op_variant)>;
            if constexpr (std::is_same_v<OperationType,
                                         bosim::operation::HomodyneSingleMode<double>>) {
                EXPECT_EQ(op_variant->Modes()[0], 1);
                EXPECT_DOUBLE_EQ(op_variant->GetTheta(), 1.3);
            } else {
                FAIL();
            }
        },
        ops[bosim::machinery::MacronodeOpIdx(5, bosim::machinery::MacronodeOpType::HomodyneA)]);
}

TEST(Machinery, MachineryCircuitLinearFFDisplacement) {
    const auto env = bosim::python::PythonEnvironment();

    auto config = TestConfig();
    auto circuit = MachineryCircuit<double>(config, 10);
    const auto& ops = circuit.GetOperations();

    // Test that the operation indices and parameters for Displacement operations (used for linear
    // FF) are correctly set on all macronodes.
    for (std::size_t k = 0; k < 6; ++k) {
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(k, bosim::machinery::MacronodeOpType::LinearFFB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(k, bosim::machinery::MacronodeOpType::LinearFFD)]);
    }

    // Test that the target modes for Displacement operations (used for linear FF) are correct on
    // macronode 3.
    {
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 4);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(3, bosim::machinery::MacronodeOpType::LinearFFB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 5);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(3, bosim::machinery::MacronodeOpType::LinearFFD)]);
    }
}

TEST(Machinery, MachineryCircuitDisplacement) {
    const auto env = bosim::python::PythonEnvironment();

    auto config = TestConfig();
    auto circuit = MachineryCircuit<double>(config, 10);
    const auto& ops = circuit.GetOperations();

    // Test that the operation indices and parameters for Displacement operations are correctly set
    // on all macronodes
    {
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                0, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                0, bosim::machinery::MacronodeOpType::DisplacementD)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                1, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                1, bosim::machinery::MacronodeOpType::DisplacementD)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                2, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.3);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.4);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                2, bosim::machinery::MacronodeOpType::DisplacementD)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.5);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.6);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::DisplacementD)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.1);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.2);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                4, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                4, bosim::machinery::MacronodeOpType::DisplacementD)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                5, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_DOUBLE_EQ(op_variant->GetX(), 0.0);
                    EXPECT_DOUBLE_EQ(op_variant->GetP(), 0.0);
                } else {
                    FAIL();
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                5, bosim::machinery::MacronodeOpType::DisplacementD)]);
    }

    // Test that the target modes for Displacement operations are correct on macronode 3."
    {
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 0);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::DisplacementB)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::Displacement<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 3);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::DisplacementD)]);
    }
}

TEST(Machinery, MachineryCircuitBS50) {
    const auto env = bosim::python::PythonEnvironment();

    auto config = TestConfig();
    auto circuit = MachineryCircuit<double>(config, 10);
    const auto& ops = circuit.GetOperations();

    // Test that the operation indices of BeamSplitter50 operations are correctly set on all
    // macronodes.
    for (std::size_t k = 0; k < 6; ++k) {
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                const bool is_bs50 =
                    std::is_same_v<OperationType, bosim::operation::util::BeamSplitter50<double>>;
                EXPECT_TRUE(is_bs50);
            },
            ops[bosim::machinery::MacronodeOpIdx(
                k, bosim::machinery::MacronodeOpType::BeamsplitterBkDk)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                const bool is_bs50 =
                    std::is_same_v<OperationType, bosim::operation::util::BeamSplitter50<double>>;
                EXPECT_TRUE(is_bs50);
            },
            ops[bosim::machinery::MacronodeOpIdx(
                k, bosim::machinery::MacronodeOpType::BeamsplitterAkBk1)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                const bool is_bs50 =
                    std::is_same_v<OperationType, bosim::operation::util::BeamSplitter50<double>>;
                EXPECT_TRUE(is_bs50);
            },
            ops[bosim::machinery::MacronodeOpIdx(
                k, bosim::machinery::MacronodeOpType::BeamsplitterCkDkn)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                const bool is_bs50 =
                    std::is_same_v<OperationType, bosim::operation::util::BeamSplitter50<double>>;
                EXPECT_TRUE(is_bs50);
            },
            ops[bosim::machinery::MacronodeOpIdx(
                k, bosim::machinery::MacronodeOpType::BeamsplitterBkAk)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                const bool is_bs50 =
                    std::is_same_v<OperationType, bosim::operation::util::BeamSplitter50<double>>;
                EXPECT_TRUE(is_bs50);
            },
            ops[bosim::machinery::MacronodeOpIdx(
                k, bosim::machinery::MacronodeOpType::BeamsplitterDkCk)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                const bool is_bs50 =
                    std::is_same_v<OperationType, bosim::operation::util::BeamSplitter50<double>>;
                EXPECT_TRUE(is_bs50);
            },
            ops[bosim::machinery::MacronodeOpIdx(
                k, bosim::machinery::MacronodeOpType::BeamsplitterDknBk1)]);
    }

    // Test whether the target modes are correct for BS50 operations on macronode 3.
    {
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::util::BeamSplitter50<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 0);
                    EXPECT_EQ(op_variant->Modes()[1], 3);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::BeamsplitterBkDk)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::util::BeamSplitter50<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 6);
                    EXPECT_EQ(op_variant->Modes()[1], 4);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::BeamsplitterAkBk1)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::util::BeamSplitter50<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 2);
                    EXPECT_EQ(op_variant->Modes()[1], 5);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::BeamsplitterCkDkn)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::util::BeamSplitter50<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 0);
                    EXPECT_EQ(op_variant->Modes()[1], 6);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::BeamsplitterBkAk)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::util::BeamSplitter50<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 3);
                    EXPECT_EQ(op_variant->Modes()[1], 2);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::BeamsplitterDkCk)]);
        std::visit(
            [](auto&& op_variant) {
                using OperationType = std::decay_t<decltype(*op_variant)>;
                if constexpr (std::is_same_v<OperationType,
                                             bosim::operation::util::BeamSplitter50<double>>) {
                    EXPECT_EQ(op_variant->Modes()[0], 5);
                    EXPECT_EQ(op_variant->Modes()[1], 4);
                }
            },
            ops[bosim::machinery::MacronodeOpIdx(
                3, bosim::machinery::MacronodeOpType::BeamsplitterDknBk1)]);
    }
}

TEST(Machinery, MachineryCircuitFF) {
    const auto env = bosim::python::PythonEnvironment();

    auto config = TestConfig();
    auto circuit = MachineryCircuit<double>(config, 10);

    const auto& ff_funcs = circuit.GetFFFuncs();

    EXPECT_EQ(ff_funcs.size(), 12);

    // From macronode 0
    {
        constexpr std::size_t SourceMacronodeIdx = 0;
        EXPECT_EQ(ff_funcs.count(bosim::machinery::MacronodeOpIdx(
                      SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD)),
                  4);
        auto range = ff_funcs.equal_range(bosim::machinery::MacronodeOpIdx(
            SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
        for (auto it = range.first; it != range.second; ++it) {
            EXPECT_EQ(it->second->source_ops_idxs[0],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneA));
            EXPECT_EQ(it->second->source_ops_idxs[1],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneB));
            EXPECT_EQ(it->second->source_ops_idxs[2],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneC));
            EXPECT_EQ(it->second->source_ops_idxs[3],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
            if (it->second->target_param.op_idx ==
                bosim::machinery::MacronodeOpIdx(SourceMacronodeIdx,
                                                 bosim::machinery::MacronodeOpType::LinearFFB)) {
                EXPECT_TRUE(it->second->target_param.param_idx == 0 ||
                            it->second->target_param.param_idx == 1);
            } else if (it->second->target_param.op_idx ==
                       bosim::machinery::MacronodeOpIdx(
                           2, bosim::machinery::MacronodeOpType::DisplacementB)) {
                EXPECT_EQ(it->second->target_param.param_idx, 1);
            } else if (it->second->target_param.op_idx ==
                       bosim::machinery::MacronodeOpIdx(
                           2, bosim::machinery::MacronodeOpType::DisplacementD)) {
                EXPECT_EQ(it->second->target_param.param_idx, 0);
            } else {
                FAIL();
            }
        }
    }

    // From macronode 1
    {
        constexpr std::size_t SourceMacronodeIdx = 1;
        EXPECT_EQ(ff_funcs.count(bosim::machinery::MacronodeOpIdx(
                      SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD)),
                  3);
        auto range = ff_funcs.equal_range(bosim::machinery::MacronodeOpIdx(
            SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
        for (auto it = range.first; it != range.second; ++it) {
            EXPECT_EQ(it->second->source_ops_idxs[0],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneA));
            EXPECT_EQ(it->second->source_ops_idxs[1],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneB));
            EXPECT_EQ(it->second->source_ops_idxs[2],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneC));
            EXPECT_EQ(it->second->source_ops_idxs[3],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
            if (it->second->target_param.op_idx ==
                bosim::machinery::MacronodeOpIdx(SourceMacronodeIdx,
                                                 bosim::machinery::MacronodeOpType::LinearFFD)) {
                EXPECT_TRUE(it->second->target_param.param_idx == 0 ||
                            it->second->target_param.param_idx == 1);
            } else if (it->second->target_param.op_idx ==
                       bosim::machinery::MacronodeOpIdx(
                           2, bosim::machinery::MacronodeOpType::HomodyneA)) {
                EXPECT_EQ(it->second->target_param.param_idx, 0);
            } else {
                FAIL();
            }
        }
    }

    // From macronode 2
    {
        constexpr std::size_t SourceMacronodeIdx = 2;
        EXPECT_EQ(ff_funcs.count(bosim::machinery::MacronodeOpIdx(
                      SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD)),
                  3);
        auto range = ff_funcs.equal_range(bosim::machinery::MacronodeOpIdx(
            SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
        for (auto it = range.first; it != range.second; ++it) {
            EXPECT_EQ(it->second->source_ops_idxs[0],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneA));
            EXPECT_EQ(it->second->source_ops_idxs[1],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneB));
            EXPECT_EQ(it->second->source_ops_idxs[2],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneC));
            EXPECT_EQ(it->second->source_ops_idxs[3],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
            if (it->second->target_param.op_idx ==
                bosim::machinery::MacronodeOpIdx(SourceMacronodeIdx,
                                                 bosim::machinery::MacronodeOpType::LinearFFB)) {
                EXPECT_TRUE(it->second->target_param.param_idx == 0 ||
                            it->second->target_param.param_idx == 1);
            } else if (it->second->target_param.op_idx ==
                       bosim::machinery::MacronodeOpIdx(
                           3, bosim::machinery::MacronodeOpType::DisplacementB)) {
                EXPECT_EQ(it->second->target_param.param_idx, 0);
            } else {
                FAIL();
            }
        }
    }

    // From macronode 3
    {
        constexpr std::size_t SourceMacronodeIdx = 3;
        EXPECT_EQ(ff_funcs.count(bosim::machinery::MacronodeOpIdx(
                      SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD)),
                  2);
        auto range = ff_funcs.equal_range(bosim::machinery::MacronodeOpIdx(
            SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
        for (auto it = range.first; it != range.second; ++it) {
            EXPECT_EQ(it->second->source_ops_idxs[0],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneA));
            EXPECT_EQ(it->second->source_ops_idxs[1],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneB));
            EXPECT_EQ(it->second->source_ops_idxs[2],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneC));
            EXPECT_EQ(it->second->source_ops_idxs[3],
                      bosim::machinery::MacronodeOpIdx(
                          SourceMacronodeIdx, bosim::machinery::MacronodeOpType::HomodyneD));
            if (it->second->target_param.op_idx ==
                bosim::machinery::MacronodeOpIdx(SourceMacronodeIdx,
                                                 bosim::machinery::MacronodeOpType::LinearFFB)) {
                EXPECT_TRUE(it->second->target_param.param_idx == 0 ||
                            it->second->target_param.param_idx == 1);
            } else {
                FAIL();
            }
        }
    }
}
