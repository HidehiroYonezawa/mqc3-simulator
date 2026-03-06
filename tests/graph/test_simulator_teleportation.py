"""Test the graph simulator (teleportation circuit)."""

from math import cos, log10, pi, sin, tan
from pathlib import Path

import pytest
from allpairspy import AllPairs
from mqc3.feedforward import feedforward
from mqc3.graph import GraphRepr
from mqc3.graph.ops import Initialization, Manual, Measurement, PhaseRotation, Squeezing

from tests.utility import check_mean_value, check_variance_value, generate_squeezed_cov, noisy_squeezed_variances

from .utility import calc_measurement_squeezing_level_in_db, run_graph_simulator

proto_output_dir = Path(__file__).parent / "proto"
n_shots = 1000
res_squeezing_level_in_db = 10
squeezing_theta = pi / 6
resource_squeezing_level_in_db = calc_measurement_squeezing_level_in_db(res_squeezing_level_in_db)
expected_noisy_variance1 = noisy_squeezed_variances(
    resource_squeezing_level_in_db,
    n_teleport=1,
    source_cov=generate_squeezed_cov(resource_squeezing_level_in_db - 10 * log10(tan(squeezing_theta) ** 2), phi=0),
)


def construct_teleportation_graph(
    displacement: tuple[float, float],
    measurement_angle: float,
    phi: float,
) -> GraphRepr:
    @feedforward
    def displace_x(m: float) -> float:
        from math import sqrt  # noqa:PLC0415

        return sqrt(2) * m

    @feedforward
    def displace_p(m: float) -> float:
        from math import sqrt  # noqa:PLC0415

        return -sqrt(2) * m

    graph_repr = GraphRepr(n_local_macronodes=7, n_steps=5)

    # Initialize mode 1 with a p-squeezed state
    graph_repr.place_operation(Initialization(macronode=(0, 1), theta=0, initialized_modes=(-1, 1)))
    # Initialize mode 2 with an x-squeezed state
    graph_repr.place_operation(Initialization(macronode=(1, 0), theta=0.5 * pi, initialized_modes=(-1, 2)))

    # R(0) to mode 1
    graph_repr.place_operation(PhaseRotation(macronode=(0, 2), swap=True, phi=0))
    # R(-pi/2) to mode 2
    graph_repr.place_operation(PhaseRotation(macronode=(1, 1), swap=False, phi=-0.5 * pi))
    # Manual(0, pi/2, pi/4, 3pi/4) to mode 1 and mode 2
    graph_repr.place_operation(
        Manual(
            macronode=(1, 2),
            swap=False,
            theta_a=0,
            theta_b=0.5 * pi,
            theta_c=0.25 * pi,
            theta_d=0.75 * pi,
        ),
    )
    # R(-pi/4) to mode 1
    graph_repr.place_operation(PhaseRotation(macronode=(2, 2), swap=False, phi=-0.25 * pi))
    # R(pi/4) to mode 2
    graph_repr.place_operation(PhaseRotation(macronode=(1, 3), swap=False, phi=0.25 * pi))

    # Initialize mode 0 with a squeezed state with squeezing angle phi
    graph_repr.place_operation(Initialization(macronode=(2, 0), theta=0.5 * pi, initialized_modes=(0, -1)))
    graph_repr.place_operation(Squeezing(macronode=(3, 0), swap=False, theta=squeezing_theta))
    graph_repr.place_operation(
        PhaseRotation(macronode=(4, 0), swap=True, phi=phi + pi / 2),  # +pi/2 is from R(-pi/2) in Squeezing
    )
    # Displacement and R(0) to mode 0
    graph_repr.place_operation(PhaseRotation(macronode=(4, 1), swap=False, phi=0, displacement_k_minus_n=displacement))
    # R(-pi/2) to mode 1
    graph_repr.place_operation(PhaseRotation(macronode=(3, 2), swap=False, phi=-0.5 * pi))
    # Manual(0, pi/2, pi/4, 3pi/4) to mode 0 and mode 1
    graph_repr.place_operation(
        Manual(
            macronode=(4, 2),
            swap=False,
            theta_a=0,
            theta_b=0.5 * pi,
            theta_c=0.25 * pi,
            theta_d=0.75 * pi,
        ),
    )
    # R(-pi/4) to mode 0
    graph_repr.place_operation(PhaseRotation(macronode=(4, 3), swap=True, phi=-0.25 * pi))
    # R(pi/4) to mode 1
    graph_repr.place_operation(PhaseRotation(macronode=(5, 2), swap=False, phi=0.25 * pi))

    # Measure x of mode 0
    graph_repr.place_operation(Measurement(macronode=(5, 3), theta=0.5 * pi))
    x0 = graph_repr.get_mode_measured_value(mode=0)
    # Measure p of mode 1
    graph_repr.place_operation(Measurement(macronode=(6, 2), theta=0.0))
    p1 = graph_repr.get_mode_measured_value(mode=1)
    # Measure mode 2
    graph_repr.place_operation(
        Measurement(
            macronode=(1, 4),
            theta=measurement_angle,
            displacement_k_minus_n=(displace_x(x0), displace_p(p1)),
        ),
    )

    return graph_repr


def simulate_graph(
    graph_repr: GraphRepr,
    n_shots: int,
    res_squeezing_level_in_db: float,
) -> list[float]:
    # Simulate the graph and get the measured values
    _, measured_values_d = run_graph_simulator(
        graph_repr=graph_repr,
        measurement_macronode=29,
        n_shots=n_shots,
        res_squeezing_level_in_db=res_squeezing_level_in_db,
    )

    return measured_values_d


def sample_measured_values(
    n_shots: int,
    res_squeezing_level_in_db: float,
    initial_displacement: tuple[float, float],
    phi: float,
    measurement_angle: float,
):
    graph = construct_teleportation_graph(
        displacement=initial_displacement,
        measurement_angle=measurement_angle,
        phi=phi,
    )

    return simulate_graph(graph, n_shots, res_squeezing_level_in_db)


@pytest.mark.parametrize(
    argnames=("x", "p", "phi"),
    argvalues=AllPairs(
        [
            [0.1, -1.0, 5.0, -20.0],
            [0.1, -1.0, 5.0, -20.0],
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
        ],
    ),
)
def test_teleportation_minor_axis(x: float, p: float, phi: float) -> None:
    displacement = (x, p)
    measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        displacement,
        phi=phi,
        measurement_angle=0.5 * pi - phi,
    )

    significance = 0.01
    check_mean_value(measured_values, x * cos(-phi) - p * sin(-phi), significance)
    check_variance_value(measured_values, expected_noisy_variance1.minor, significance)


@pytest.mark.parametrize(
    argnames=("x", "p", "phi"),
    argvalues=AllPairs(
        [
            [0.1, -1.0, 5.0, -20.0],
            [0.1, -1.0, 5.0, -20.0],
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
        ],
    ),
)
def test_teleportation_major_axis(x: float, p: float, phi: float) -> None:
    displacement = (x, p)
    n_shots = 3000
    measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        displacement,
        phi=phi,
        measurement_angle=-phi,
    )

    significance = 0.01
    check_mean_value(measured_values, x * sin(-phi) + p * cos(-phi), significance)
    check_variance_value(measured_values, expected_noisy_variance1.major, significance)


@pytest.mark.parametrize(
    argnames=("x", "p", "phi"),
    argvalues=AllPairs(
        [
            [0.1, -1.0, 5.0, -20.0],
            [0.1, -1.0, 5.0, -20.0],
            [-3 * pi / 4, -pi / 4, 0, pi / 6, pi / 3, pi / 2, pi],
        ],
    ),
)
def test_teleportation_oblique_axis(x: float, p: float, phi: float) -> None:
    displacement = (x, p)
    n_shots = 3000
    measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        displacement,
        phi=phi,
        measurement_angle=pi / 4 - phi,
    )

    significance = 0.01
    check_mean_value(measured_values, x * cos(-phi - pi / 4) - p * sin(-phi - pi / 4), significance)
    check_variance_value(measured_values, expected_noisy_variance1.oblique_45, significance)
