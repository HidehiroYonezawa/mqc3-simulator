"""Test the graph simulator (unitary operations)."""

from pathlib import Path

import numpy as np
import pytest
from mqc3.circuit import CircuitRepr
from mqc3.circuit import Operation as CircuitOperation
from mqc3.circuit.ops import intrinsic
from mqc3.graph import GraphRepr
from mqc3.graph.ops import (
    Initialization,
    Measurement,
    PhaseRotation,
    ShearPInvariant,
    ShearXInvariant,
    Squeezing,
    Squeezing45,
)
from mqc3.graph.ops import Operation as GraphOperation

from .utility import (
    check_normal_distribution,
    initialize_mode,
    run_circuit_simulator,
    run_graph_simulator,
)

proto_output_dir = Path(__file__).parent / "proto"
n_shots = 1000
res_squeezing_level_in_db = 10
initialization_measurement_angle = 0


def construct_graph(measurement_angle: float, unitary_op: GraphOperation | None = None) -> GraphRepr:
    # Construct a graph
    graph_repr = GraphRepr(n_local_macronodes=3, n_steps=1)
    graph_repr.place_operation(
        Initialization(macronode=(0, 0), theta=initialization_measurement_angle, initialized_modes=(0, -1)),
    )
    if unitary_op is not None:
        graph_repr.place_operation(unitary_op)
    graph_repr.place_operation(Measurement(macronode=(2, 0), theta=measurement_angle))
    return graph_repr


def simulate_graph(
    graph_repr: GraphRepr,
    n_shots: int,
    res_squeezing_level_in_db: float,
) -> list[float]:
    # Simulate the graph and get the measured values
    measured_values_b, _ = run_graph_simulator(
        graph_repr=graph_repr,
        measurement_macronode=2,
        n_shots=n_shots,
        res_squeezing_level_in_db=res_squeezing_level_in_db,
    )

    return measured_values_b


def construct_circuit(res_squeezing_level_in_db: float, unitary_op: CircuitOperation | None = None) -> CircuitRepr:
    # Construct a circuit
    circuit = CircuitRepr("test_circuit")
    if unitary_op is None:
        circuit.Q(0)
    else:
        circuit.Q(0) | unitary_op  # type: ignore  # noqa: PGH003
    initialize_mode(circuit, 0, res_squeezing_level_in_db, initialization_measurement_angle - 0.5 * np.pi)
    return circuit


def simulate_circuit(circuit: CircuitRepr):
    # Simulate the circuit and get the mean and covariance matrix
    state = run_circuit_simulator(circuit=circuit)
    gaussian = state.get_gaussian_state(i=0)
    return gaussian.mean, gaussian.cov


def calc_expected_state(
    res_squeezing_level_in_db: float,
    unitary_op: CircuitOperation | None = None,
):
    circuit = construct_circuit(res_squeezing_level_in_db, unitary_op)
    expected_mean, expected_cov = simulate_circuit(circuit)
    return expected_mean, expected_cov


def sample_measured_values(
    n_shots: int,
    res_squeezing_level_in_db: float,
    unitary_op: GraphOperation | None = None,
):
    x_graph = construct_graph(0.5 * np.pi, unitary_op)
    x_measured_values = simulate_graph(x_graph, n_shots, res_squeezing_level_in_db)

    p_graph = construct_graph(0.0, unitary_op)
    p_measured_values = simulate_graph(p_graph, n_shots, res_squeezing_level_in_db)

    return x_measured_values, p_measured_values


def test_initialization() -> None:
    circuit_op = None
    graph_op = None

    expected_mean, expected_cov = calc_expected_state(res_squeezing_level_in_db, circuit_op)
    assert np.isreal(expected_mean).all()

    x_measured_values, p_measured_values = sample_measured_values(n_shots, res_squeezing_level_in_db, graph_op)
    assert check_normal_distribution(x_measured_values, expected_mean[0].real, expected_cov[0, 0])
    assert check_normal_distribution(p_measured_values, expected_mean[1].real, expected_cov[1, 1])


@pytest.mark.parametrize("phi", [0, 0.25 * np.pi, -0.5 * np.pi, np.pi])
def test_phase_rotation(phi: float) -> None:
    circuit_op = intrinsic.PhaseRotation(phi=phi)
    graph_op = PhaseRotation(macronode=(1, 0), phi=phi, swap=False)

    expected_mean, expected_cov = calc_expected_state(res_squeezing_level_in_db, circuit_op)
    assert np.isreal(expected_mean).all()

    x_measured_values, p_measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        graph_op,
    )
    assert check_normal_distribution(x_measured_values, expected_mean[0].real, expected_cov[0, 0])
    assert check_normal_distribution(p_measured_values, expected_mean[1].real, expected_cov[1, 1])


@pytest.mark.parametrize("kappa", [0, 1.0, -2.0, 3.0])
def test_shear_x_invariant(kappa: float) -> None:
    circuit_op = intrinsic.ShearXInvariant(kappa=kappa)
    graph_op = ShearXInvariant(macronode=(1, 0), kappa=kappa, swap=False)

    expected_mean, expected_cov = calc_expected_state(res_squeezing_level_in_db, circuit_op)
    assert np.isreal(expected_mean).all()

    x_measured_values, p_measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        graph_op,
    )
    assert check_normal_distribution(x_measured_values, expected_mean[0].real, expected_cov[0, 0])
    assert check_normal_distribution(p_measured_values, expected_mean[1].real, expected_cov[1, 1])


@pytest.mark.parametrize("eta", [0, 1.0, -2.0, 3.0])
def test_shear_p_invariant(eta: float) -> None:
    circuit_op = intrinsic.ShearPInvariant(eta=eta)
    graph_op = ShearPInvariant(macronode=(1, 0), eta=eta, swap=False)

    expected_mean, expected_cov = calc_expected_state(res_squeezing_level_in_db, circuit_op)
    assert np.isreal(expected_mean).all()

    x_measured_values, p_measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        graph_op,
    )
    assert check_normal_distribution(x_measured_values, expected_mean[0].real, expected_cov[0, 0])
    assert check_normal_distribution(p_measured_values, expected_mean[1].real, expected_cov[1, 1])


@pytest.mark.parametrize("theta", [0.125 * np.pi, -0.25 * np.pi, 0.75 * np.pi])
def test_squeezing(theta: float) -> None:
    circuit_op = intrinsic.Squeezing(theta=theta)
    graph_op = Squeezing(macronode=(1, 0), theta=theta, swap=False)

    expected_mean, expected_cov = calc_expected_state(res_squeezing_level_in_db, circuit_op)
    assert np.isreal(expected_mean).all()

    x_measured_values, p_measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        graph_op,
    )
    assert check_normal_distribution(x_measured_values, expected_mean[0].real, expected_cov[0, 0])
    assert check_normal_distribution(p_measured_values, expected_mean[1].real, expected_cov[1, 1])


@pytest.mark.parametrize("theta", [0.125 * np.pi, -0.25 * np.pi, 0.75 * np.pi])
def test_squeezing45(theta: float) -> None:
    circuit_op = intrinsic.Squeezing45(theta=theta)
    graph_op = Squeezing45(macronode=(1, 0), theta=theta, swap=False)

    expected_mean, expected_cov = calc_expected_state(res_squeezing_level_in_db, circuit_op)
    assert np.isreal(expected_mean).all()

    x_measured_values, p_measured_values = sample_measured_values(
        n_shots,
        res_squeezing_level_in_db,
        graph_op,
    )
    assert check_normal_distribution(x_measured_values, expected_mean[0].real, expected_cov[0, 0])
    assert check_normal_distribution(p_measured_values, expected_mean[1].real, expected_cov[1, 1])
