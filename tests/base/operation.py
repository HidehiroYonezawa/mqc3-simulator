# pyright: reportUnusedExpression=false

"""Operation class test."""

import json
import sys
from cmath import polar
from math import cos, isclose, log, pi, sin
from pathlib import Path
from statistics import compare_1d_data

import numpy as np
import strawberryfields as sf
from state import compare_states, compare_states_mean_cov, juxtapose_wigner_plots
from strawberryfields.ops import BSgate, Catstate, MeasureHomodyne, Squeezed

sf.hbar = 1

registry = {}


def command(func):
    registry[func.__name__] = func
    return func


def B_ab(alpha, beta):
    cos_p = cos(alpha + beta)
    cos_m = cos(alpha - beta)
    sin_p = sin(alpha + beta)
    sin_m = sin(alpha - beta)
    return np.array([
        [cos_p * cos_m, sin_p * sin_m, -sin_p * cos_m, sin_m * cos_p],
        [sin_p * sin_m, cos_p * cos_m, sin_m * cos_p, -sin_p * cos_m],
        [sin_p * cos_m, -sin_m * cos_p, cos_p * cos_m, sin_p * sin_m],
        [-sin_m * cos_p, sin_p * cos_m, sin_p * sin_m, cos_p * cos_m],
    ])


def R12(t1, t2):
    return np.array([
        [cos(t1), 0, -sin(t1), 0],
        [0, cos(t2), 0, -sin(t2)],
        [sin(t1), 0, cos(t1), 0],
        [0, sin(t2), 0, cos(t2)],
    ])


def B_std(theta, phi):
    return np.array(R12(pi - theta - phi, pi / 2 - theta) @ B_ab(0, theta) @ R12(phi - pi, -pi / 2))


@command
def test_python(a, b):
    assert int(a) * 2 == int(b)


@command
def test_bs(json_path: str) -> None:
    """Compare the input and theoretical beam splitter matrices element-wise.

    The input beam splitter operation matrix is compared element-wise
    with the theoretically equivalent operation matrix.

    It is assumed that JSON data in the following format is provided as input.
    {
        "bs": {"theta": float}
        "matrix": array[float],
    }

    Args:
        json_path (str): Input JSON path.
    """
    with open(json_path, encoding="utf-8") as file:
        bosim_data = json.load(file)
    theta = bosim_data["bs"]["theta"]

    B_flat = np.ravel(B_ab(0, theta))
    assert len(bosim_data["matrix"]) == len(B_flat)
    for ele_sim, ele_ideal in zip(bosim_data["matrix"], B_flat, strict=False):
        assert isclose(ele_sim, ele_ideal, abs_tol=1e-6)


class SingleSqueezedMeasurementTestData:
    def __init__(self, bosim_data: dict) -> None:
        self.squeezing_level = bosim_data["state"]["squeezing_level"]
        self.phi = bosim_data["state"]["phi"]
        self.theta = bosim_data["state"]["theta"]
        if "select" in bosim_data:
            self.select = bosim_data["select"]
        if "measured_vals" in bosim_data:
            self.measured_vals = bosim_data["measured_vals"]


@command
def test_1squeezed_homodyne(json_path: str) -> None:
    """Compare measured squeezed state with Strawberry Fields results.

    The measured values of squeezed state are given as input and compared with those
    obtained from the same process in Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
        "state": {"real": float, "imag": float},
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "coeff": {"real": float, "imag": float},
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        },
        "select": float,
        "measured_vals": [float...]
    }

    Args:
        json_path (str): Input JSON path.
    """
    with open(json_path, encoding="utf-8") as file:
        bosim_data = json.load(file)
    bosim_dict = SingleSqueezedMeasurementTestData(bosim_data)

    prog = sf.Program(2)
    # Preparing two modes because an error occurs when performing a measurement in a single-mode circuit
    # in Strawberry Fields.
    with prog.context as q:
        r = bosim_dict.squeezing_level * log(10) / 10
        Squeezed(r=r / 2, p=bosim_dict.phi * 2) | q[0]
        MeasureHomodyne(phi=pi / 2 - bosim_dict.theta, select=bosim_dict.select) | q[0]

    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state
    compare_states_mean_cov(state, bosim_data)
    # When measuring a quantum system where all states are in a single mode,
    # Strawberry Fields does not update the weights.

    measured_vals = []
    for i in range(10000):
        prog = sf.Program(1)
        np.random.seed(i)
        with prog.context as q:
            r = bosim_dict.squeezing_level * log(10) / 10
            Squeezed(r=r / 2, p=bosim_dict.phi * 2) | q
            MeasureHomodyne(phi=pi / 2 - bosim_dict.theta) | q
        eng = sf.Engine("bosonic")
        result = eng.run(prog, shots=1)
        measured_vals.append(result.samples[0][0])

    compare_1d_data(measured_vals, bosim_dict.measured_vals)


class SingleCatMeasurementTestData:
    def __init__(self, bosim_data: dict) -> None:
        config_state = complex(bosim_data["state"]["real"], bosim_data["state"]["imag"])
        self.state_a, self.state_phi = polar(config_state)
        if "select" in bosim_data:
            self.select = bosim_data["select"]
        if "measured_vals" in bosim_data:
            self.measured_vals = bosim_data["measured_vals"]


@command
def test_1cat_homodyne(json_path: str) -> None:
    """Compare measured cat state with Strawberry Fields results.

    The measured values of cat state are given as input and compared with those
    obtained from the same process in Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
        "state": {"real": float, "imag": float},
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "coeff": {"real": float, "imag": float},
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        },
        "select": float,
        "measured_vals": [float...]
    }

    Args:
        json_path (str): Input JSON path.
    """
    with open(json_path, encoding="utf-8") as file:
        bosim_data = json.load(file)
    bosim_dict = SingleCatMeasurementTestData(bosim_data)

    prog = sf.Program(2)
    # Preparing two modes because an error occurs when performing a measurement in a single-mode circuit
    # in Strawberry Fields.
    with prog.context as q:
        Catstate(a=bosim_dict.state_a, phi=bosim_dict.state_phi) | q[0]
        MeasureHomodyne(phi=0, select=bosim_dict.select) | q[0]

    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state
    compare_states_mean_cov(state, bosim_data)
    # When measuring a quantum system where all states are in a single mode,
    # Strawberry Fields does not update the weights.

    measured_vals = []
    for i in range(1000):
        prog = sf.Program(1)
        np.random.seed(i)
        with prog.context as q:
            Catstate(a=bosim_dict.state_a, phi=bosim_dict.state_phi) | q
            MeasureHomodyne(phi=0) | q
        eng = sf.Engine("bosonic")
        result = eng.run(prog, shots=1)
        measured_vals.append(result.samples[0][0])

    compare_1d_data(measured_vals, bosim_dict.measured_vals)


class TwoCatsBsTestData:
    def __init__(self, bosim_data: dict) -> None:
        self.theta = bosim_data["bs"]["theta"]
        self.phi = bosim_data["bs"]["phi"]
        config_state0 = complex(bosim_data["state0"]["real"], bosim_data["state0"]["imag"])
        config_state1 = complex(bosim_data["state1"]["real"], bosim_data["state1"]["imag"])
        self.state0_a, self.state0_phi = polar(config_state0)
        self.state1_a, self.state1_phi = polar(config_state1)
        if "select" in bosim_data:
            self.select = bosim_data["select"]
        if "measured_vals" in bosim_data:
            self.measured_vals = bosim_data["measured_vals"]


@command
def test_two_cats_bs(json_path: str) -> None:
    """Compare the input beam-splitter-applied cat states with those from Strawberry Fields.

    The state obtained by applying a beam splitter to two cat states is given as input and
    compared with the state obtained by performing the same operation in Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
        "bs": {"theta": float, "phi": float},
        "state0": {"real": float, "imag": float},
        "state1": {"real": float, "imag": float},
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "coeff": {"real": float, "imag": float},
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        }
    }

    Args:
        json_path (str): Input JSON path.
    """
    with open(json_path, encoding="utf-8") as file:
        bosim_data = json.load(file)
    bosim_dict = TwoCatsBsTestData(bosim_data)

    prog = sf.Program(2)
    with prog.context as q:
        Catstate(a=bosim_dict.state0_a, phi=bosim_dict.state0_phi) | q[0]
        Catstate(a=bosim_dict.state1_a, phi=bosim_dict.state1_phi) | q[1]
        BSgate(theta=bosim_dict.theta, phi=bosim_dict.phi) | (q[0], q[1])
    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state

    juxtapose_wigner_plots(state, bosim_data, fig_dir=Path(json_path).parent, fig_name=Path(json_path).stem)
    compare_states(state, bosim_data)


@command
def test_two_cats_bs_homodyne(json_path: str) -> None:
    """Compare measured beam-splitter-applied cat states with Strawberry Fields results.

    The measured values of two cat states interacting via a beam splitter are given as input
    and compared with those obtained from the same process in Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
        "bs": {"theta": float, "phi": float},
        "state0": {"real": float, "imag": float},
        "state1": {"real": float, "imag": float},
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "coeff": {"real": float, "imag": float},
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        },
        "select": {0: float},
        "measured_vals": [float...]
    }

    Args:
        json_path (str): Input JSON path.
    """
    with open(json_path, encoding="utf-8") as file:
        bosim_data = json.load(file)
    bosim_dict = TwoCatsBsTestData(bosim_data)

    prog = sf.Program(2)
    with prog.context as q:
        Catstate(a=bosim_dict.state0_a, phi=bosim_dict.state0_phi) | q[0]
        Catstate(a=bosim_dict.state1_a, phi=bosim_dict.state1_phi) | q[1]
        BSgate(theta=bosim_dict.theta, phi=bosim_dict.phi) | (q[0], q[1])
        MeasureHomodyne(phi=0, select=bosim_dict.select[0]) | q[0]
    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state

    juxtapose_wigner_plots(state, bosim_data, fig_dir=Path(json_path).parent, fig_name=Path(json_path).stem)
    compare_states(state, bosim_data)

    measured_vals = []
    for i in range(1000):
        np.random.seed(i)
        prog = sf.Program(2)
        with prog.context as q:
            Catstate(a=bosim_dict.state0_a, phi=bosim_dict.state0_phi) | q[0]
            Catstate(a=bosim_dict.state1_a, phi=bosim_dict.state1_phi) | q[1]
            BSgate(theta=bosim_dict.theta, phi=bosim_dict.phi) | (q[0], q[1])
            MeasureHomodyne(phi=0) | q[0]
        eng = sf.Engine("bosonic")
        result = eng.run(prog, shots=1)
        measured_vals.append(result.samples[0][0])

    compare_1d_data(measured_vals, bosim_dict.measured_vals)


@command
def test_two_cats_bs_homodyne_theta(json_path):
    """Compare measured beam-splitter-applied cat states with Strawberry Fields results.

    The measured values of two cat states interacting via a beam splitter are given as input
    and compared with those obtained from the same process in Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
        "bs": {"theta": float, "phi": float},
        "state0": {"real": float, "imag": float},
        "state1": {"real": float, "imag": float},
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "coeff": {"real": float, "imag": float},
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        },
        "select": {0: float},
        "measured_vals": [float...]
    }

    Args:
        json_path (str): Input JSON path.
    """
    with open(json_path, encoding="utf-8") as file:
        bosim_data = json.load(file)
    bosim_dict = TwoCatsBsTestData(bosim_data)

    homodyne_angle_theta = bosim_data["homodyne_angle_theta"]

    prog = sf.Program(2)
    with prog.context as q:
        Catstate(a=bosim_dict.state0_a, phi=bosim_dict.state0_phi) | q[0]
        Catstate(a=bosim_dict.state1_a, phi=bosim_dict.state1_phi) | q[1]
        BSgate(theta=bosim_dict.theta, phi=bosim_dict.phi) | (q[0], q[1])
        (MeasureHomodyne(phi=pi / 2 - homodyne_angle_theta, select=bosim_dict.select[0]) | q[0])
    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state

    juxtapose_wigner_plots(state, bosim_data, fig_dir=Path(json_path).parent, fig_name=Path(json_path).stem)
    compare_states(state, bosim_data)

    measured_vals = []
    for i in range(1000):
        np.random.seed(i)
        prog = sf.Program(2)
        with prog.context as q:
            Catstate(a=bosim_dict.state0_a, phi=bosim_dict.state0_phi) | q[0]
            Catstate(a=bosim_dict.state1_a, phi=bosim_dict.state1_phi) | q[1]
            BSgate(theta=bosim_dict.theta, phi=bosim_dict.phi) | (q[0], q[1])
            (MeasureHomodyne(phi=pi / 2 - homodyne_angle_theta) | q[0])
        eng = sf.Engine("bosonic")
        result = eng.run(prog, shots=1)
        measured_vals.append(result.samples[0][0])

    compare_1d_data(measured_vals, bosim_dict.measured_vals)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise ValueError("Usage: python your_script.py <function_name> [args...]")

    func_name = sys.argv[1]
    args = sys.argv[2:]

    assert func_name in registry
    result = registry[func_name](*args)
    if result is not None:
        print(result)
