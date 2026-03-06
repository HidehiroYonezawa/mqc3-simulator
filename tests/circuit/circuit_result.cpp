#include <gtest/gtest.h>

#include "bosim/circuit.h"
#include "bosim/simulate/circuit_repr.h"
#include "bosim/state.h"

TEST(CircuitResult, SetMeasuredValues) {
    auto circuit = bosim::Circuit<double>{};
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(3, 0);
    circuit.EmplaceOperation<bosim::operation::ShearXInvariant<double>>(1, 0.5);
    circuit.EmplaceOperation<bosim::operation::HomodyneSingleMode<double>>(1, 1.57);
    const auto measured_values = std::map<std::size_t, double>{{0, 1.0}, {2, -2.0}};
    auto pb_shot_measured_value = bosim::circuit::PbShotMeasuredValue{};
    bosim::circuit::SetMeasuredValues<double>(circuit, measured_values, &pb_shot_measured_value);
    EXPECT_EQ(pb_shot_measured_value.measured_vals_size(), 2);
    EXPECT_EQ(pb_shot_measured_value.measured_vals().at(0).index(), 3);
    EXPECT_DOUBLE_EQ(pb_shot_measured_value.measured_vals().at(0).value(), 1.0);
    EXPECT_EQ(pb_shot_measured_value.measured_vals().at(1).index(), 1);
    EXPECT_DOUBLE_EQ(pb_shot_measured_value.measured_vals().at(1).value(), -2.0);
}

TEST(CircuitResult, SetState) {
    const auto state = bosim::State<double>::GenerateCoherent(std::complex<double>{1, 2});
    auto pb_bosonic_state = bosim::circuit::PbBosonicState{};
    bosim::circuit::SetState<double>(state, &pb_bosonic_state);
}
