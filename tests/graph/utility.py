"""Utility functions for tests for graph representation."""

# Import _mqc3_simulator_impl module
try:
    from mqc3_simulator._mqc3_simulator_impl import simulate_circuit, simulate_graph  # type: ignore  # noqa: PGH003
except (ImportError, ModuleNotFoundError):
    import sys
    from pathlib import Path

    import pytest

    sys.path.append(str((Path(__file__).parent / ".." / ".." / "build" / "src").resolve()))
    pytest.importorskip(modname="_mqc3_simulator_impl", reason="Missing the bosonic simulator Python bindings.")
    from _mqc3_simulator_impl import simulate_circuit, simulate_graph  # type: ignore  # noqa: PGH003, PLC2701

import numpy as np
from mqc3.circuit import BosonicState, CircuitRepr, GaussianState
from mqc3.graph import GraphRepr, GraphResult
from mqc3.pb.mqc3_cloud.program.v1.graph_pb2 import GraphResult as PbGraphResult
from mqc3.pb.mqc3_cloud.program.v1.quantum_program_pb2 import QuantumProgramResult as PbQuantumProgramResult
from scipy.stats import kstest

from tests.utility import generate_squeezed_cov


def run_graph_simulator(
    graph_repr: GraphRepr,
    measurement_macronode: int,
    n_shots: int,
    res_squeezing_level_in_db: float,
) -> tuple[list[float], list[float]]:
    pb_graph_bytes = graph_repr.proto().SerializePartialToString()
    pb_result_bytes = simulate_graph(pb_graph_bytes, "multi_cpu", n_shots, res_squeezing_level_in_db)
    pb_result = PbGraphResult()
    pb_result.ParseFromString(pb_result_bytes)
    graph_result = GraphResult.construct_from_proto(pb_result)

    measured_values_b = [shot[measurement_macronode].m_b for shot in graph_result.measured_vals]
    measured_values_d = [shot[measurement_macronode].m_d for shot in graph_result.measured_vals]
    return measured_values_b, measured_values_d


def run_circuit_simulator(
    circuit: CircuitRepr,
) -> BosonicState:
    pb_circuit_bytes = circuit.proto().SerializePartialToString()
    pb_result_bytes = simulate_circuit(pb_circuit_bytes, "single_cpu", 1, "all")
    pb_result = PbQuantumProgramResult()
    pb_result.ParseFromString(pb_result_bytes)
    state = BosonicState.construct_from_proto(pb_result.circuit_state[0])
    if state is None:
        msg = "State is None"
        raise RuntimeError(msg)
    return state


def calc_measurement_squeezing_level_in_db(res_squeezing_level_in_db: float) -> float:
    return res_squeezing_level_in_db - 10 * np.log10(2)


def calc_squeezing_level_no_unit(squeezing_level_in_db: float) -> float:
    return 10.0 ** (squeezing_level_in_db / 10.0)


def initialize_mode(circuit: CircuitRepr, mode: int, res_squeezing_level_in_db: float, squeezing_angle: float) -> None:
    squeezing_level_in_db = calc_measurement_squeezing_level_in_db(res_squeezing_level_in_db)
    mean = np.zeros(2, dtype=np.complex128)
    cov = generate_squeezed_cov(squeezing_level_in_db, squeezing_angle)
    gaussian_squeezed_state = GaussianState(mean, cov)
    bosonic_squeezed_state = BosonicState(
        coeffs=np.array([1.0], dtype=np.complex128),
        gaussian_states=[gaussian_squeezed_state],
    )
    circuit.set_initial_state(index=mode, state=bosonic_squeezed_state)


def check_normal_distribution(
    data: list[float],
    expected_mean: float,
    expected_variance: float,
    alpha: float = 0.05,
) -> bool:
    expected_std = np.sqrt(expected_variance)
    _, p_value = kstest(rvs=data, cdf="norm", args=(expected_mean, expected_std), method="exact")
    return p_value > alpha
