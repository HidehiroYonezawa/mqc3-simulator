import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd
from matplotlib import axes


def load_benchmark_data(path: Path) -> pd.DataFrame:
    return pd.read_csv(path)


def plot(ax: axes.Axes, df: pd.DataFrame, fixed_param: tuple[str, float], x_col: str, y_cols: list[str]) -> None:
    fixed_field, fixed_value = fixed_param
    filtered = df[df[fixed_field] == fixed_value].sort_values(x_col)
    for col in y_cols:
        ax.plot(filtered[x_col], filtered[col], label=col, marker="o")

    ax.set_ylim(3e-1, 2e4)

    ax.set_title(f"{fixed_field} = {fixed_value}", fontsize=16)
    ax.set_xlabel(x_col, fontsize=14)
    ax.set_ylabel("Time [ms]", fontsize=14)
    ax.set_xscale("log")
    ax.set_yscale("log")
    ax.set_box_aspect(1)

    ax.tick_params(axis="x", which="major", labelsize=12)
    ax.tick_params(axis="y", which="major", labelsize=12)
    ax.tick_params(axis="x", which="minor", labelbottom=False)

    ax.grid(visible=True)


def calc_predicted_circuit_simulation_ms(
    n_local_macronodes: int,
    n_steps: int,
    bs50_op_ms: float,
    measurement_op_ms: float,
) -> float:
    n_bs50_ops = n_local_macronodes * n_steps * 6
    n_measurement_ops = n_local_macronodes * n_steps * 4
    return bs50_op_ms * n_bs50_ops + measurement_op_ms * n_measurement_ops


def plot_predicted_circuit_simulation_time(
    ax: axes.Axes,
    n_local_macronodes: int,
    all_n_steps: list[int],
    bs50_op_ms: float,
    measurement_op_ms: float,
) -> None:
    predicted = [
        calc_predicted_circuit_simulation_ms(n_local_macronodes, n_steps, bs50_op_ms, measurement_op_ms)
        for n_steps in all_n_steps
    ]
    ax.plot(all_n_steps, predicted, label="Circuit simulation (predicted)", linestyle="--")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Plot benchmark results from CSV file")
    parser.add_argument("csv_path", type=str, help="Path to the benchmark CSV file")
    parser.add_argument("output_path", type=str, help="Path to the output image file")
    args = parser.parse_args()

    data = load_benchmark_data(Path(args.csv_path))

    fig, (ax1) = plt.subplots(1, 1, figsize=(12, 6))

    columns = [
        "InitializeMachineryModes",
        "ConvertMachineryToCircuit",
        "SimulateCPUSingleThread",
        "ExtractMachineryResult",
    ]

    plot(ax1, data, ("n_local_macronodes", 101), "n_steps", columns)

    all_n_steps = sorted(data["n_steps"].unique())
    plot_predicted_circuit_simulation_time(
        ax=ax1,
        n_local_macronodes=101,
        all_n_steps=all_n_steps,
        bs50_op_ms=0.00431,
        measurement_op_ms=0.03994,
    )

    plt.legend(bbox_to_anchor=(1.02, 1), loc="upper left", borderaxespad=0)
    plt.tight_layout()
    ax1.relim()
    ax1.autoscale()

    plt.tight_layout()
    plt.savefig(args.output_path, bbox_inches="tight")
