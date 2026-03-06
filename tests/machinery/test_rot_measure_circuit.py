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


from math import pi

from allpairspy import AllPairs
from mqc3.graph import GraphRepr
from mqc3.graph.ops import Measurement, PhaseRotation
from mqc3.machinery import MachineryRepr
from mqc3.machinery.result import MachineryResult
from mqc3.pb.mqc3_cloud.program.v1.machinery_pb2 import (
    MachineryResult as PbMachineryResult,
)

from tests.utility import check_mean_value, check_variance_value, noisy_squeezed_variances


def run_circuit(
    n_shots: int,
    squeezing_level: float,
    phi: float,
    *,
    swap: bool,
    measurement_angle: float,
) -> tuple[list[float], list[float]]:
    """Runs a circuit with the given parameters and returns the results.

    Args:
        n_shots (int): Number of shots.
        squeezing_level (float): Squeezing level in dB.
        phi (float): Rotation angle in radians.
        swap (bool): Swap parameter in the rotation macronode.
        measurement_angle (float): Measurement angle in radians.

    Returns:
        tuple[list[float], list[float]]: A tuple containing the results of the circuit,
            corresponding to micronodes b and d.
    """
    graph_repr = GraphRepr(1, 2)
    graph_repr.place_operation(PhaseRotation(macronode=(0, 0), phi=phi, swap=swap))
    graph_repr.place_operation(Measurement(macronode=(0, 1), theta=measurement_angle))

    machinery_repr = MachineryRepr.from_graph_repr(graph_repr)
    pb_machinery_bytes = machinery_repr.proto().SerializeToString()
    pb_result_bytes = simulate_machinery(pb_machinery_bytes, "multi_cpu", n_shots, squeezing_level)
    pb_result = PbMachineryResult()
    pb_result.ParseFromString(pb_result_bytes)
    machinery_result = MachineryResult.construct_from_proto(pb_result)

    results_b = []
    results_d = []
    for smv in machinery_result.measured_vals:
        mmv = smv[1]
        results_b.append(mmv.m_b)
        results_d.append(mmv.m_d)

    return results_b, results_d


@pytest.mark.parametrize(
    argnames=("phi", "swap"),
    argvalues=AllPairs(
        [
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
            [True, False],
        ],
    ),
)
def test_rot_measure_circuit(phi: float, *, swap: bool) -> None:
    """Perform a phase rotation and measurement, then analyze the resulting data.

    This test executes a phase rotation macronode, followed by a measurement macronode,
    using feedforward processing. It measures micronodes 'b' and 'd'
    within the measurement macronode and extracts the measured values as 1D data.
    Finally, it verifies the following:

    1.  The mean is 0.
    2.  The variance matches the theoretical expected value for each measurement angle.
        The variance is checked for the following angles:
        - Major axis direction
        - Minor axis direction
        - 45° diagonal direction between the major and minor axes.

    Args:
        phi (float): Rotation x-p angle.
        swap (bool): If the output modes from the rotation macronode are swapped.
    """
    n_shots = 1000
    squeezing_level = 15
    expected_variance = noisy_squeezed_variances(squeezing_level)

    # Measurement along the minor axis
    results_b, results_d = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        phi=phi,
        swap=swap,
        measurement_angle=pi / 2 - phi,
    )
    check_mean_value(results_b, expected_mean=0.0, significance=0.01)
    check_mean_value(results_d, expected_mean=0.0, significance=0.01)
    check_variance_value(results_b, expected_variance=expected_variance.minor, significance=0.01)
    check_variance_value(results_d, expected_variance=expected_variance.minor, significance=0.01)

    # Measurement along the major axis
    results_b, results_d = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        phi=phi,
        swap=swap,
        measurement_angle=pi / 2 - (phi + pi / 2),
    )
    check_mean_value(results_b, expected_mean=0.0, significance=0.01)
    check_mean_value(results_d, expected_mean=0.0, significance=0.01)
    check_variance_value(results_b, expected_variance=expected_variance.major, significance=0.01)
    check_variance_value(results_d, expected_variance=expected_variance.major, significance=0.01)

    # Measurement along the oblique axis
    results_b, results_d = run_circuit(
        n_shots=n_shots,
        squeezing_level=squeezing_level,
        phi=phi,
        swap=swap,
        measurement_angle=pi / 2 - (phi + pi / 4),
    )
    check_mean_value(results_b, expected_mean=0.0, significance=0.01)
    check_mean_value(results_d, expected_mean=0.0, significance=0.01)
    check_variance_value(results_b, expected_variance=expected_variance.oblique_45, significance=0.01)
    check_variance_value(results_d, expected_variance=expected_variance.oblique_45, significance=0.01)
