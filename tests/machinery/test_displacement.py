"""Test simple gate teleportation circuits."""

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

from math import cos, pi, sin

from allpairspy import AllPairs
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

dx1 = 1.0
dp1 = 2.0
dx2 = 3.0
dp2 = 4.0
dx3 = 5.0
dp3 = 6.0


def run_circuit(
    n_shots: int,
    squeezing_level: float,
    phi1: float,
    phi2: float,
    *,
    swap: bool,
    init_angle: float,
    measurement_angle: float,
) -> tuple[list[float], list[float], list[float]]:
    """Runs a circuit with the given parameters and returns the results.

    Args:
        n_shots (int): Number of shots.
        squeezing_level (float): Squeezing level in dB.
        phi1 (float): Rotation angle in the first rotation macronode in radians.
        phi2 (float): Rotation angle in the second rotation macronode in radians.
        swap (bool): Swap parameter in the first rotation macronode.
        init_angle (float): Initialization angle in radians.
        measurement_angle (float): Measurement angle in radians.

    Returns:
        tuple[list[float], list[float], list[float]]: A tuple containing the results of the circuit,
            corresponding to micronodes b of the initialization macronode and b and d of the measurement macronode.
    """
    graph_repr = GraphRepr(2, 2)
    graph_repr.place_operation(PhaseRotation(macronode=(0, 0), phi=phi1, swap=swap))
    graph_repr.place_operation(
        Initialization(
            macronode=(1, 0),
            theta=init_angle,
            initialized_modes=(0, 1),
            displacement_k_minus_1=(dx1, dp1),
            readout=True,
        ),
    )
    graph_repr.place_operation(
        PhaseRotation(macronode=(0, 1), phi=phi2, swap=True, displacement_k_minus_n=(dx2, dp2)),
    )
    graph_repr.place_operation(
        Measurement(macronode=(1, 1), theta=measurement_angle, displacement_k_minus_n=(dx3, dp3)),
    )

    machinery_repr = MachineryRepr.from_graph_repr(graph_repr)
    pb_machinery_bytes = machinery_repr.proto().SerializeToString()
    pb_result_bytes = simulate_machinery(pb_machinery_bytes, "single_cpu", n_shots, squeezing_level)
    pb_result = PbMachineryResult()
    pb_result.ParseFromString(pb_result_bytes)
    machinery_result = MachineryResult.construct_from_proto(pb_result)

    results_ib = []
    results_mb = []
    results_md = []
    for smv in machinery_result.measured_vals:
        mmv1 = smv[1]
        mmv3 = smv[3]
        results_ib.append(mmv1.m_b)
        results_mb.append(mmv3.m_b)
        results_md.append(mmv3.m_d)

    return results_ib, results_mb, results_md


def coordinate_projection(x: float, p: float, theta: float):
    return x * sin(theta) + p * cos(theta)


@pytest.mark.parametrize(
    argnames=("phi1", "swap"),
    argvalues=AllPairs(
        [
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
            [True, False],
        ],
    ),
)
def test_displacement_minor_axis(phi1: float, *, swap: bool) -> None:
    """Perform displacement test circuit and analyze the resulting data.

    The machinery representation consists of phase rotation, initialization,
    phase rotation, and measurement macronodes in order.

    The micronode 'b' in the initialization macronode and
    the micronodes 'b' and 'd' in the measurement macronode are measured.
    The measured values are extracted as 1D data for each micronode.
    Finally, the test verifies the following:

    1.  The means are as expected.
    2.  The variances match the theoretical expected values for each measurement angle.
        They are checked along the major axes.

    Args:
        phi1 (float): Rotation x-p angle of the first rotation macronode.
        swap (bool): If the output modes from the rotation macronode are swapped.
    """
    n_shots = 1000
    squeezing_level = 15
    expected_noisy_variance1 = noisy_squeezed_variances(squeezing_level, n_teleport=1)
    expected_noisy_variance2 = noisy_squeezed_variances(squeezing_level, n_teleport=2)
    expected_init_variance = initialized_squeezed_variances(squeezing_level)

    theta1 = pi / 2 - phi1
    theta2 = -theta1
    initialized_phi = theta1 - pi / 2
    phi2 = initialized_phi - phi1
    results_ib, results_mb, results_md = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        phi1=phi1,
        phi2=phi2,
        swap=swap,
        init_angle=theta1,
        measurement_angle=theta2,
    )
    check_mean_value(results_ib, expected_mean=coordinate_projection(dx1, dp1, theta1), significance=0.01)
    check_mean_value(
        results_mb,
        expected_mean=coordinate_projection(
            cos(phi2) * dx2 - sin(phi2) * dp2,
            sin(phi2) * dx2 + cos(phi2) * dp2,
            theta2,
        ),
        significance=0.01,
    )
    check_mean_value(results_md, expected_mean=coordinate_projection(dx3, dp3, theta2), significance=0.01)
    check_variance_value(results_ib, expected_variance=expected_noisy_variance1.minor, significance=0.01)
    check_variance_value(results_mb, expected_variance=expected_noisy_variance2.minor, significance=0.01)
    check_variance_value(results_md, expected_variance=expected_init_variance.minor, significance=0.01)


@pytest.mark.parametrize(
    argnames=("phi1", "swap"),
    argvalues=AllPairs(
        [
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
            [True, False],
        ],
    ),
)
def test_displacement_major_axis(phi1: float, *, swap: bool) -> None:
    """Perform displacement test circuit and analyze the resulting data.

    The machinery representation consists of phase rotation, initialization,
    phase rotation, and measurement macronodes in order.

    The micronode 'b' in the initialization macronode and
    the micronodes 'b' and 'd' in the measurement macronode are measured.
    The measured values are extracted as 1D data for each micronode.
    Finally, the test verifies the following:

    1.  The means are as expected.
    2.  The variances match the theoretical expected values for each measurement angle.
        They are checked along the minor axes.

    Args:
        phi1 (float): Rotation x-p angle of the first rotation macronode.
        swap (bool): If the output modes from the rotation macronode are swapped.
    """
    n_shots = 1000
    squeezing_level = 15
    expected_noisy_variance1 = noisy_squeezed_variances(squeezing_level, n_teleport=1)
    expected_noisy_variance2 = noisy_squeezed_variances(squeezing_level, n_teleport=2)
    expected_init_variance = initialized_squeezed_variances(squeezing_level)

    theta1 = -phi1
    theta2 = pi / 2 - theta1
    initialized_phi = theta1 - pi / 2
    phi2 = initialized_phi - phi1
    results_ib, results_mb, results_md = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        phi1=phi1,
        phi2=phi2,
        swap=swap,
        init_angle=theta1,
        measurement_angle=theta2,
    )
    check_mean_value(results_ib, expected_mean=coordinate_projection(dx1, dp1, theta1), significance=0.01)
    check_mean_value(
        results_mb,
        expected_mean=coordinate_projection(
            cos(phi2) * dx2 - sin(phi2) * dp2,
            sin(phi2) * dx2 + cos(phi2) * dp2,
            theta2,
        ),
        significance=0.01,
    )
    check_mean_value(results_md, expected_mean=coordinate_projection(dx3, dp3, theta2), significance=0.01)
    check_variance_value(results_ib, expected_variance=expected_noisy_variance1.major, significance=0.01)
    check_variance_value(results_mb, expected_variance=expected_noisy_variance2.major, significance=0.01)
    check_variance_value(results_md, expected_variance=expected_init_variance.major, significance=0.01)


@pytest.mark.parametrize(
    argnames=("phi1", "swap"),
    argvalues=AllPairs(
        [
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
            [True, False],
        ],
    ),
)
def test_displacement_oblique_axis(phi1: float, *, swap: bool) -> None:
    """Perform displacement test circuit and analyze the resulting data.

    The machinery representation consists of phase rotation, initialization,
    phase rotation, and measurement macronodes in order.

    The micronode 'b' in the initialization macronode and
    the micronodes 'b' and 'd' in the measurement macronode are measured.
    The measured values are extracted as 1D data for each micronode.
    Finally, the test verifies the following:

    1.  The means are as expected.
    2.  The variances match the theoretical expected values for each measurement angle.
        They are checked along the 45° diagonal direction between the major and minor axes.

    Args:
        phi1 (float): Rotation x-p angle of the first rotation macronode.
        swap (bool): If the output modes from the rotation macronode are swapped.
    """
    n_shots = 1000
    squeezing_level = 15
    expected_noisy_variance1 = noisy_squeezed_variances(squeezing_level, n_teleport=1)
    expected_noisy_variance2 = noisy_squeezed_variances(squeezing_level, n_teleport=2)
    expected_init_variance = initialized_squeezed_variances(squeezing_level)

    theta1 = pi / 4 - phi1
    theta2 = pi / 4 - theta1
    initialized_phi = theta1 - pi / 2
    phi2 = initialized_phi - phi1
    results_ib, results_mb, results_md = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        phi1=phi1,
        phi2=phi2,
        swap=swap,
        init_angle=theta1,
        measurement_angle=theta2,
    )
    check_mean_value(results_ib, expected_mean=coordinate_projection(dx1, dp1, theta1), significance=0.01)
    check_mean_value(
        results_mb,
        expected_mean=coordinate_projection(
            cos(phi2) * dx2 - sin(phi2) * dp2,
            sin(phi2) * dx2 + cos(phi2) * dp2,
            theta2,
        ),
        significance=0.005,
    )
    check_mean_value(results_md, expected_mean=coordinate_projection(dx3, dp3, theta2), significance=0.01)
    check_variance_value(results_ib, expected_variance=expected_noisy_variance1.oblique_45, significance=0.01)
    check_variance_value(results_mb, expected_variance=expected_noisy_variance2.oblique_45, significance=0.01)
    check_variance_value(results_md, expected_variance=expected_init_variance.oblique_45, significance=0.01)
