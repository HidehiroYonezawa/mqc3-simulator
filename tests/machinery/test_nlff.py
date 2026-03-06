"""Test non-linear feedforward."""

import pytest

# Import _mqc3_simulator_impl module
try:
    from mqc3_simulator._mqc3_simulator_impl import simulate_machinery  # type: ignore  # noqa: PGH003
except (ImportError, ModuleNotFoundError):
    import sys
    from pathlib import Path

    sys.path.append(str((Path(__file__).parent / ".." / ".." / "build" / "src").resolve()))
    pytest.importorskip(modname="_mqc3_simulator_impl", reason="Missing the bosonic simulator Python bindings.")
    from _mqc3_simulator_impl import simulate_machinery  # type: ignore  # noqa: PGH003, PLC2701

from enum import Enum
from math import pi

from mqc3.feedforward import feedforward
from mqc3.graph import GraphRepr
from mqc3.graph.ops import Initialization, Measurement, PhaseRotation
from mqc3.machinery import MachineryRepr
from mqc3.machinery.result import MachineryResult
from mqc3.pb.mqc3_cloud.program.v1.machinery_pb2 import (
    MachineryResult as PbMachineryResult,
)

from tests.utility import (
    check_mean_value,
    check_variance_value,
    initialized_squeezed_variances,
    noisy_squeezed_variances,
)


@feedforward
def ff_half_m(m: float) -> float:
    return m * 0.5


@feedforward
def ff_half_m_plus90(m: float) -> float:
    from math import pi

    return (m + pi) * 0.5


@feedforward
def ff_measurement_0(m: float) -> float:
    from math import pi

    return pi / 2 - m


@feedforward
def ff_measurement_45(m: float) -> float:
    from math import pi

    return pi / 4 - m


@feedforward
def ff_measurement_90(m: float) -> float:
    return -m


@feedforward
def ff_as_is(m: float) -> float:
    return m


@feedforward
def ff_plus_90(m: float) -> float:
    from math import pi

    return m + pi / 2


class MeasurementAxis(Enum):
    MINOR = 0
    MAJOR = 1
    OBLIQUE = 2


class RotationMethod(Enum):
    PHASE_ROTATION = 0
    INITIALIZATION = 1


def run_circuit(
    n_shots: int,
    squeezing_level: float,
    measurement_axis: MeasurementAxis,
    rotation_method: RotationMethod,
) -> list[float]:
    """Runs a circuit with the given parameters and returns the results.

    Args:
        n_shots (int): Number of shots.
        squeezing_level (float): Squeezing level in dB.
        measurement_axis (MeasurementAxis): Measurement angle.
        rotation_method (RotationMethod): Rotation method at macronode 2.

    Returns:
        list[float]: Results of the circuit, corresponding to micronodes b and d.
    """
    graph_repr = GraphRepr(4, 1)
    graph_repr.place_operation(
        Initialization(
            macronode=(0, 0),
            theta=0,
            initialized_modes=(0, -1),
        ),
    )
    graph_repr.place_operation(Measurement(macronode=(1, 0), theta=pi / 2, readout=True))
    m = graph_repr.get_mode_measured_value(0)
    if rotation_method == RotationMethod.PHASE_ROTATION:
        graph_repr.place_operation(PhaseRotation(macronode=(2, 0), phi=ff_as_is(m), swap=True))
    elif rotation_method == RotationMethod.INITIALIZATION:
        graph_repr.place_operation(
            Initialization(macronode=(2, 0), theta=ff_as_is(ff_plus_90(m)), initialized_modes=(1, -1)),
        )
    if measurement_axis == MeasurementAxis.MINOR:
        graph_repr.place_operation(Measurement(macronode=(3, 0), theta=ff_measurement_0(m)))
    elif measurement_axis == MeasurementAxis.MAJOR:
        graph_repr.place_operation(Measurement(macronode=(3, 0), theta=ff_measurement_90(m)))
    elif measurement_axis == MeasurementAxis.OBLIQUE:
        graph_repr.place_operation(Measurement(macronode=(3, 0), theta=ff_measurement_45(m)))

    machinery_repr = MachineryRepr.from_graph_repr(graph_repr)
    pb_machinery_bytes = machinery_repr.proto().SerializeToString()
    pb_result_bytes = simulate_machinery(pb_machinery_bytes, "multi_cpu_shot", n_shots, squeezing_level)
    pb_result = PbMachineryResult()
    pb_result.ParseFromString(pb_result_bytes)
    machinery_result = MachineryResult.construct_from_proto(pb_result)

    results = []
    for smv in machinery_result.measured_vals:
        mmv = smv[3]
        results.append(mmv.m_b)

    return results


@pytest.mark.parametrize("rotation_method", [RotationMethod.PHASE_ROTATION, RotationMethod.INITIALIZATION])
def test_nlff_minor_axis(rotation_method: RotationMethod) -> None:
    """Test the nonlinear feedforward (NLFF) along the major axis.

    This test initializes a mode and measures it.
    Then, it initialized another mode and measures it again.
    The second initialization angle and the last measurement angle is conditioned on the first measurement.
    The test extracts the measured values as 1D data and verifies the following:

    1.  The mean is 0.
    2.  The variance matches the theoretical expected value for each measurement angle.
        The variance is checked for the following angles major axis direction.
    """
    n_shots = 1000
    squeezing_level = 15
    if rotation_method == RotationMethod.PHASE_ROTATION:
        expected_variance = noisy_squeezed_variances(squeezing_level)
    elif rotation_method == RotationMethod.INITIALIZATION:
        expected_variance = initialized_squeezed_variances(squeezing_level)

    # Measurement along the minor axis
    results = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        measurement_axis=MeasurementAxis.MINOR,
        rotation_method=rotation_method,
    )
    check_mean_value(results, expected_mean=0.0, significance=0.01)
    check_variance_value(results, expected_variance=expected_variance.minor, significance=0.01)


@pytest.mark.parametrize("rotation_method", [RotationMethod.PHASE_ROTATION, RotationMethod.INITIALIZATION])
def test_nlff_major_axis(rotation_method: RotationMethod) -> None:
    """Test the nonlinear feedforward (NLFF) along the major axis.

    This test initializes a mode and measures it.
    Then, it initialized another mode and measures it again.
    The second initialization angle and the last measurement angle is conditioned on the first measurement.
    The test extracts the measured values as 1D data and verifies the following:

    1.  The mean is 0.
    2.  The variance matches the theoretical expected value for each measurement angle.
        The variance is checked for the following angles minor axis direction.
    """
    n_shots = 1000
    squeezing_level = 15
    if rotation_method == RotationMethod.PHASE_ROTATION:
        expected_variance = noisy_squeezed_variances(squeezing_level)
    elif rotation_method == RotationMethod.INITIALIZATION:
        expected_variance = initialized_squeezed_variances(squeezing_level)

    # Measurement along the minor axis
    results = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        measurement_axis=MeasurementAxis.MAJOR,
        rotation_method=rotation_method,
    )
    check_mean_value(results, expected_mean=0.0, significance=0.01)
    check_variance_value(results, expected_variance=expected_variance.major, significance=0.01)


@pytest.mark.parametrize("rotation_method", [RotationMethod.PHASE_ROTATION, RotationMethod.INITIALIZATION])
def test_nlff_oblique_axis(rotation_method: RotationMethod) -> None:
    """Test the nonlinear feedforward (NLFF) along the oblique axis.

    This test initializes a mode and measures it.
    Then, it initialized another mode and measures it again.
    The second initialization angle and the last measurement angle is conditioned on the first measurement.
    The test extracts the measured values as 1D data and verifies the following:

    1.  The mean is 0.
    2.  The variance matches the theoretical expected value for each measurement angle.
        The variance is checked for the 45° diagonal direction between the major and minor axes.
    """
    n_shots = 1000
    squeezing_level = 15
    if rotation_method == RotationMethod.PHASE_ROTATION:
        expected_variance = noisy_squeezed_variances(squeezing_level)
    elif rotation_method == RotationMethod.INITIALIZATION:
        expected_variance = initialized_squeezed_variances(squeezing_level)

    # Measurement along the minor axis
    results = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        measurement_axis=MeasurementAxis.OBLIQUE,
        rotation_method=rotation_method,
    )
    check_mean_value(results, expected_mean=0.0, significance=0.01)
    check_variance_value(results, expected_variance=expected_variance.oblique_45, significance=0.01)
