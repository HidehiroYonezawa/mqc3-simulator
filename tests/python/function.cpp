#include "bosim/python/function.h"

#include <boost/math/distributions/normal.hpp>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <numbers>

#include "bosim/circuit.h"
#include "bosim/exception.h"
#include "bosim/python/manager.h"
#include "bosim/simulate/simulate.h"

static constexpr auto Pi = std::numbers::pi_v<double>;
static constexpr double SqueezingLevel = 100.0;

constexpr auto SquareFunc = R"(def call_square(x):
    return x * x)";
constexpr auto SinFunc = R"(def call_sin(x):
    from math import sin
    return sin(x))";
constexpr auto AddFunc = R"(def call_add(x, y):
    return x + y)";
constexpr auto F1Func = R"(def call_f1(x):
    return x * x)";
constexpr auto F2Func = R"(def call_f2(x):
    return x + 10)";
constexpr auto F3Func = R"(def call_f3(x):
    return -x
)";
double Composite(double x) { return -(x * x + 10); }
constexpr auto C1Func = R"(def call_f1(x, y):
    return x * y, x / y)";
constexpr auto C2Func = R"(def call_f2(x, y):
    return x + y, x - y, 5)";
constexpr auto C3Func = R"(def call_f3(x, y, z):
    return -(x + y + z)
)";
double CompositeComplex(double x, double y) { return -2 * x * y - 5; }
constexpr auto FuncErrorCase1 = R"(def call_f(x, y):
    return x * y)";
constexpr auto FuncErrorCase2A = R"(def call_f1(x):
    return x + 10, x - 10)";
constexpr auto FuncErrorCase2B = R"(def call_f2(x):
    return -x
)";

constexpr auto FFFuncX = R"(def calc_x(x):
    from math import sqrt
    ret = sqrt(2) * x
    return ret)";
constexpr auto FFFuncP = R"(def calc_p(p):
    from math import sqrt
    ret = -sqrt(2) * p
    return ret)";

auto CreateTeleportationCircuit(double x, double p)
    -> std::pair<bosim::Circuit<double>, bosim::State<double>> {
    // Circuit
    auto circuit = bosim::Circuit<double>{};

    // BS
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, 0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(2, -Pi / 2.0);
    circuit.EmplaceOperation<bosim::operation::Manual<double>>(1, 2, 0, Pi / 2, Pi / 4,
                                                               3.0 * Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(2, Pi / 4.0);

    // BS
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, 0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, -Pi / 2.0);
    circuit.EmplaceOperation<bosim::operation::Manual<double>>(0, 1, 0, Pi / 2, Pi / 4,
                                                               3.0 * Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(0, -Pi / 4.0);
    circuit.EmplaceOperation<bosim::operation::PhaseRotation<double>>(1, Pi / 4.0);

    // Measure
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(0, Pi / 2);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(1, 0);
    circuit.EmplaceOperation<bosim::operation::Displacement<double>>(2, 0, 0);

    auto ff_func_x = bosim::python::PythonFeedForward({FFFuncX});
    auto ff_func_p = bosim::python::PythonFeedForward({FFFuncP});
    // const auto ff_func_x = bosim::FFFunc<double>{[](const auto& x) -> double { return
    // std::sqrt(2) * x[0]; }}; const auto ff_func_p = bosim::FFFunc<double>{[](const auto& p) ->
    // double { return -std::sqrt(2) * p[0]; }};
    auto ff_x = bosim::FeedForward<double>{{10}, {12, 0}, ff_func_x};
    circuit.AddFF(ff_x);
    auto ff_p = bosim::FeedForward<double>{{11}, {12, 1}, ff_func_p};
    circuit.AddFF(ff_p);

    // State
    auto state = bosim::State<double>{
        {bosim::SingleModeMultiPeak<double>::GenerateSqueezed(0, 0, std::complex<double>{x, p}),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel, Pi / 2),
         bosim::SingleModeMultiPeak<double>::GenerateSqueezed(SqueezingLevel)}};
    return {std::move(circuit), std::move(state)};
}

TEST(PythonFeedForward, Square) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto square = bosim::python::PythonFeedForward({SquareFunc});
        EXPECT_DOUBLE_EQ(1.44, square({1.2}));
        EXPECT_DOUBLE_EQ(9.0, square({3.0}));
    }
}
TEST(PythonFeedForward, Sin) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto sin = bosim::python::PythonFeedForward({SinFunc});
        EXPECT_DOUBLE_EQ(0.0, sin({0.0}));
        EXPECT_DOUBLE_EQ(1.0, sin({Pi / 2}));
        EXPECT_NEAR(0.0, sin({Pi}), 1e-15);
    }
}
TEST(PythonFeedForward, Add) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto add = bosim::python::PythonFeedForward({AddFunc});
        EXPECT_DOUBLE_EQ(8.0, add({-2.0, 10.0}));
    }
}
TEST(PythonFeedForward, Composite) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto f = bosim::python::PythonFeedForward({F1Func, F2Func, F3Func});
        EXPECT_DOUBLE_EQ(Composite(0.0), f({0.0}));
    }
}
TEST(PythonFeedForward, CompositeComplex) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto f = bosim::python::PythonFeedForward({C1Func, C2Func, C3Func});
        EXPECT_DOUBLE_EQ(CompositeComplex(3.0, 1.0), f({3.0, 1.0}));
    }
}
TEST(PythonFeedForwardError, Case1) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto f = bosim::python::PythonFeedForward({FuncErrorCase1});
        f({0.0, 0.0});
        EXPECT_THROW(f({0.0}), bosim::SimulationError);
    }
}
TEST(PythonFeedForwardError, Case2) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto f = bosim::python::PythonFeedForward({FuncErrorCase2A, FuncErrorCase2B});
        EXPECT_THROW(f({0.0}), bosim::SimulationError);
    }
}
TEST(PythonFeedForward, SimulateTeleportation) {
    constexpr std::size_t NShots = 10;
    const auto env = bosim::python::PythonEnvironment();

    for (const auto backend :
         {bosim::Backend::CPUSingleThread, bosim::Backend::CPUMultiThreadShotLevel,
          bosim::Backend::CPUMultiThreadPeakLevel, bosim::Backend::CPUMultiThread}) {
        const auto settings = bosim::SimulateSettings{NShots, backend, bosim::StateSaveMethod::All};

        for (const auto& x : {1.5, -20.0, 0.01}) {
            for (const auto& p : {1.5, -20.0, -0.01}) {
                auto [circuit, state] = CreateTeleportationCircuit(x, p);
                const auto result = bosim::Simulate<double>(settings, circuit, state);
                for (std::size_t shot = 0; shot < NShots; ++shot) {
                    const auto& mean = result.result[shot].state.value().GetMean(0);
                    EXPECT_NEAR(x, mean(2).real(), 2 * 1e-5);
                    EXPECT_NEAR(p, mean(5).real(), 2 * 1e-5);
                }
            }
        }
    }
}

TEST(ConvertToFFFunc, Square) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto python_square = bosim::python::PythonFeedForward({SquareFunc});
        const auto square = bosim::python::ConvertToFFFunc<double>(python_square);
        EXPECT_DOUBLE_EQ(1.44, square({1.2}));
        EXPECT_DOUBLE_EQ(9.0, square({3.0}));
    }
}
TEST(ConvertToFFFunc, Sin) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto python_sin = bosim::python::PythonFeedForward({SinFunc});
        const auto sin = bosim::python::ConvertToFFFunc<double>(python_sin);
        EXPECT_DOUBLE_EQ(0.0, sin({0.0}));
        EXPECT_DOUBLE_EQ(1.0, sin({Pi / 2}));
        EXPECT_NEAR(0.0, sin({Pi}), 1e-15);
    }
}
TEST(ConvertToFFFunc, Add) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto python_add = bosim::python::PythonFeedForward({AddFunc});
        const auto add = bosim::python::ConvertToFFFunc<float>(python_add);
        EXPECT_FLOAT_EQ(8.0f, add({-2.0f, 10.0f}));
    }
}
TEST(ConvertToFFFunc, Composite) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto python_f = bosim::python::PythonFeedForward({F1Func, F2Func, F3Func});
        const auto f = bosim::python::ConvertToFFFunc<float>(python_f);
        EXPECT_FLOAT_EQ(Composite(0.0f), f({0.0f}));
    }
}
TEST(ConvertToFFFunc, CompositeComplex) {
    const auto env = bosim::python::PythonEnvironment();
    {
        const auto python_f = bosim::python::PythonFeedForward({C1Func, C2Func, C3Func});
        const auto f = bosim::python::ConvertToFFFunc<double>(python_f);
        EXPECT_DOUBLE_EQ(CompositeComplex(3.0, 1.0), f({3.0, 1.0}));
    }
}
