# ruff: noqa: INP001,S101

# pyright: reportUnusedExpression=false

"""Circuit class test."""

import json
import sys
from math import pi
from statistics import check_mean_value, check_variance_value

import matplotlib.pyplot as plt
import strawberryfields as sf

sf.hbar = 1

registry = {}


def command(func):
    registry[func.__name__] = func
    return func


def draw_hists(hists: list[list], labels: list[str], xlabel: str, filename_suffix: str = "") -> None:
    """Draw histograms of 1-dimensional datasets.

    Args:
        hists (list[list]): List of 1-dimensional datasets
        labels (list[str]): List of labels corresponding to hists.
        xlabel (str): Label of x-axis.
        filename_suffix (str): Optional suffix of output file name

    Raises:
        ValueError: labels needs to have the same length as hists.
    """
    if len(hists) != len(labels):
        msg = "The number of labels must match the number of histograms"
        raise ValueError(msg)
    for h, label in zip(hists, labels, strict=False):
        plt.hist(h, bins=30, alpha=0.5, label=label)
    plt.xlabel(xlabel)
    plt.legend()
    if not filename_suffix:
        plt.savefig("plot.png")
    else:
        plt.savefig(f"plot_{filename_suffix}.png")


@command
def test_teleportation(json_path):
    with open(json_path, encoding="utf-8") as file:
        sim_data = json.load(file)

    sample_x = sim_data["sample_x"]
    sample_p = sim_data["sample_p"]
    squeezing_level = sim_data["squeezing_level"]
    phi = sim_data["phi"]

    draw_hists(
        hists=[sample_x, sample_p],
        labels=[f"$phi = {phi / pi:.3f}pi$", f"$phi = {(phi + pi / 2) / pi:.3f}pi$"],
        xlabel="Measured value",
    )

    check_mean_value(data=sample_x, expected=0)

    squeezing_level_no_unit = 10.0 ** (squeezing_level * 0.1)
    initial_x_var = sf.hbar / (2 * squeezing_level_no_unit)
    initial_p_var = sf.hbar * squeezing_level_no_unit / 2
    check_variance_value(data=sample_x, expected=initial_x_var * 3)
    check_variance_value(data=sample_p, expected=initial_p_var + initial_x_var * 2)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise ValueError("Usage: python your_script.py <function_name> [args...]")

    func_name = sys.argv[1]
    args = sys.argv[2:]

    assert func_name in registry
    result = registry[func_name](*args)
    if result is not None:
        print(result)
