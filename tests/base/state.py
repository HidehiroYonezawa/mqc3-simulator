# pyright: reportUnusedExpression=false

"""State class test."""

import json
import sys
from cmath import polar
from math import isclose, log
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import strawberryfields as sf
from strawberryfields.ops import Catstate, Squeezed

sf.hbar = 1

registry = {}


def command(func):
    registry[func.__name__] = func
    return func


def compare_states(state, bosim_data: dict) -> None:
    """Compare the input Strawberry Fields state object with the bosim state data to verify if they match.

    It is assumed that JSON data in the following format is provided as input.
    {
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "coeff": {"real": float, "imag": float},
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        }
    }

    Args:
        state (BaseState): State object of Strawberry Fields.
        bosim_data (dict): Input JSON data.
    """
    assert len(state.weights()) == len(bosim_data["peaks"])
    assert bosim_data["n_peaks"] == len(bosim_data["peaks"])
    n_modes = bosim_data["n_modes"]
    for m, (sf_c, sf_mu, sf_cov, bosim_peak) in enumerate(
        zip(
            state.weights(),
            state.means(),
            state.covs(),
            bosim_data["peaks"],
            strict=False,
        ),
    ):
        assert isclose(sf_c.real, bosim_peak["coeff"]["real"], abs_tol=1e-6)
        assert isclose(sf_c.imag, bosim_peak["coeff"]["imag"], abs_tol=1e-6)
        for i in range(n_modes):
            assert isclose(sf_mu[2 * i].real, bosim_peak["mean"][i]["real"], abs_tol=1e-6)
            assert isclose(sf_mu[2 * i].imag, bosim_peak["mean"][i]["imag"], abs_tol=1e-6)
            assert isclose(
                sf_mu[2 * i + 1].real,
                bosim_peak["mean"][i + n_modes]["real"],
                abs_tol=1e-6,
            )
            assert isclose(
                sf_mu[2 * i + 1].imag,
                bosim_peak["mean"][i + n_modes]["imag"],
                abs_tol=1e-6,
            )
            for j in range(n_modes):
                assert isclose(
                    sf_cov[2 * i][2 * j],
                    bosim_peak["cov"][2 * n_modes * i + j],
                    abs_tol=1e-6,
                )
                assert isclose(
                    sf_cov[2 * i][2 * j + 1],
                    bosim_peak["cov"][2 * n_modes * i + (j + n_modes)],
                    abs_tol=1e-6,
                )
                assert isclose(
                    sf_cov[2 * i + 1][2 * j],
                    bosim_peak["cov"][2 * n_modes * (i + n_modes) + j],
                    abs_tol=1e-6,
                )
                assert isclose(
                    sf_cov[2 * i + 1][2 * j + 1],
                    bosim_peak["cov"][2 * n_modes * (i + n_modes) + (j + n_modes)],
                    abs_tol=1e-6,
                )


def compare_states_mean_cov(state, bosim_data: dict) -> None:
    """Compare the input Strawberry Fields state object with the bosim state data to verify if they match.

    Peak coefficients are not compared.

    It is assumed that JSON data in the following format is provided as input.
    {
        "n_modes": int,
        "n_peaks": int,
        "peaks": {
            "mean": [{"real": float, "imag": float}...],
            "cov": [float...]
        }
    }

    Args:
        state (BaseState): State object of Strawberry Fields.
        bosim_data (dict): Input JSON data.
    """
    assert len(state.weights()) == len(bosim_data["peaks"])
    n_modes = bosim_data["n_modes"]
    for m, (sf_mu, sf_cov, bosim_peak) in enumerate(
        zip(
            state.means(),
            state.covs(),
            bosim_data["peaks"],
            strict=False,
        ),
    ):
        for i in range(n_modes):
            assert isclose(sf_mu[2 * i].real, bosim_peak["mean"][i]["real"], abs_tol=1e-6)
            assert isclose(sf_mu[2 * i].imag, bosim_peak["mean"][i]["imag"], abs_tol=1e-6)
            assert isclose(
                sf_mu[2 * i + 1].real,
                bosim_peak["mean"][i + n_modes]["real"],
                abs_tol=1e-6,
            )
            assert isclose(
                sf_mu[2 * i + 1].imag,
                bosim_peak["mean"][i + n_modes]["imag"],
                abs_tol=1e-6,
            )
            for j in range(n_modes):
                assert isclose(
                    sf_cov[2 * i][2 * j],
                    bosim_peak["cov"][2 * n_modes * i + j],
                    abs_tol=1e-6,
                )
                assert isclose(
                    sf_cov[2 * i][2 * j + 1],
                    bosim_peak["cov"][2 * n_modes * i + (j + n_modes)],
                    abs_tol=1e-6,
                )
                assert isclose(
                    sf_cov[2 * i + 1][2 * j],
                    bosim_peak["cov"][2 * n_modes * (i + n_modes) + j],
                    abs_tol=1e-6,
                )
                assert isclose(
                    sf_cov[2 * i + 1][2 * j + 1],
                    bosim_peak["cov"][2 * n_modes * (i + n_modes) + (j + n_modes)],
                    abs_tol=1e-6,
                )


def plot_wigner_bosim(sim_data, fig_dir, fig_name):
    sim_wigner = sim_data["wigner"]
    xvec = np.linspace(sim_wigner["start"], sim_wigner["stop"], sim_wigner["num"])
    modes = list(map(int, sim_wigner["vals"].keys()))

    for mode in modes:
        fig, ax = plt.subplots(figsize=(6, 6))
        sim_vals = np.array(sim_wigner["vals"][str(mode)]).reshape(sim_wigner["num"], sim_wigner["num"])
        scale = np.max(np.abs(sim_vals))
        contour = ax.contourf(xvec, xvec, sim_vals, 60, cmap="RdBu", vmin=-scale, vmax=scale)
        ax.set_aspect("equal")
        ax.set_title(f"bosim: mode {mode}")
        fig.colorbar(contour, ax=ax)

        plt.tight_layout()
        fig_path = fig_dir / f"{fig_name}_mode{mode}.png"
        plt.savefig(fig_path)
        print(f"Output figure: {fig_path}")


@command
def simply_plot_wigner_bosim(json_path):
    with open(json_path, encoding="utf-8") as file:
        sim_data = json.load(file)

    plot_wigner_bosim(sim_data, fig_dir=Path(json_path).parent, fig_name=Path(json_path).stem)


def juxtapose_wigner_plots(state, sim_data, fig_dir, fig_name):
    sim_wigner = sim_data["wigner"]
    xvec = np.linspace(sim_wigner["start"], sim_wigner["stop"], sim_wigner["num"])
    modes = list(map(int, sim_wigner["vals"].keys()))

    for mode in modes:
        fig, axes = plt.subplots(1, 2, figsize=(12, 6))

        sim_vals = np.array(sim_wigner["vals"][str(mode)]).reshape(sim_wigner["num"], sim_wigner["num"])
        scale1 = np.max(np.abs(sim_vals))
        contour1 = axes[0].contourf(xvec, xvec, sim_vals, 60, cmap="RdBu", vmin=-scale1, vmax=scale1)
        axes[0].set_aspect("equal")
        axes[0].set_title("bosim")
        fig.colorbar(contour1, ax=axes[0])

        sf_vals_complex = state.wigner(mode, xvec=xvec, pvec=xvec)
        sf_vals = np.round(sf_vals_complex.real, 4)
        scale2 = np.max(np.abs(sf_vals))
        contour2 = axes[1].contourf(xvec, xvec, sf_vals, 60, cmap="RdBu", vmin=-scale2, vmax=scale2)
        axes[1].set_aspect("equal")
        axes[1].set_title("Strawberry Fields")
        fig.colorbar(contour2, ax=axes[1])

        plt.tight_layout()
        fig_path = fig_dir / f"{fig_name}_mode{mode}.png"
        plt.savefig(fig_path)
        print(f"Output figure: {fig_path}")


@command
def test_squeezed(json_path):
    with open(json_path, encoding="utf-8") as file:
        sim_data = json.load(file)
    displacement = complex(
        sim_data["state"]["displacement"]["real"],
        sim_data["state"]["displacement"]["imag"],
    )
    # TODO Add displacement test
    if displacement != complex(0.0, 0.0):
        raise NotImplementedError("Displacement cannot be tested")

    squeezing_level = sim_data["state"]["squeezing_level"]
    phi = sim_data["state"]["phi"]

    prog = sf.Program(1)
    with prog.context as q:
        r = squeezing_level * log(10) / 10
        Squeezed(r=r / 2, p=phi * 2) | q
    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state
    juxtapose_wigner_plots(state, sim_data, fig_dir=Path(json_path).parent, fig_name=Path(json_path).stem)
    compare_states(state, sim_data)


@command
def test_1cat(json_path):
    """Compare the input cat state with the one generated using Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
        "state": {"real": float, "imag": float},
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
        sim_data = json.load(file)
    config_state = complex(sim_data["state"]["real"], sim_data["state"]["imag"])
    parity = sim_data["state"].get("parity")
    state_a, state_phi = polar(config_state)

    prog = sf.Program(1)
    with prog.context as q:
        if parity is None:
            Catstate(a=state_a, phi=state_phi) | q
        else:
            Catstate(a=state_a, phi=state_phi, p=parity) | q
    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state
    compare_states(state, sim_data)


@command
def test_two_cats(json_path):
    """Compare the input two-mode cat state with the one generated using Strawberry Fields.

    It is assumed that JSON data in the following format is provided as input.
    {
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
    config_state0 = complex(bosim_data["state0"]["real"], bosim_data["state0"]["imag"])
    config_state1 = complex(bosim_data["state1"]["real"], bosim_data["state1"]["imag"])
    state0_a, state0_phi = polar(config_state0)
    state1_a, state1_phi = polar(config_state1)

    prog = sf.Program(2)
    with prog.context as q:
        Catstate(a=state0_a, phi=state0_phi) | q[0]
        Catstate(a=state1_a, phi=state1_phi) | q[1]
    eng = sf.Engine("bosonic")
    result = eng.run(prog, shots=1)
    state = result.state

    juxtapose_wigner_plots(state, bosim_data, fig_dir=Path(json_path).parent, fig_name=Path(json_path).stem)
    compare_states(state, bosim_data)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise ValueError("Usage: python your_script.py <function_name> [args...]")

    func_name = sys.argv[1]
    args = sys.argv[2:]

    assert func_name in registry
    result = registry[func_name](*args)
    if result is not None:
        print(result)
