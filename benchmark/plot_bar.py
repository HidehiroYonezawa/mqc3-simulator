# ruff: noqa: T201
import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.lines import Line2D

current_dir = Path.cwd()

parser = argparse.ArgumentParser(description="Commandline options for benchmarking")
parser.add_argument("--unary", action="store_true", help="Execute unary operations benchmark")
parser.add_argument("--binary", action="store_true", help="Execute binary operations benchmark")
parser.add_argument("--measurement", action="store_true", help="Execute measurement operations benchmark")
parser.add_argument(
    "--unary-input",
    type=str,
    help="Input csv file path of unary operations benchmark plot",
    default=f"{current_dir}/benchmark_result_unary.csv",
)
parser.add_argument(
    "--binary-input",
    type=str,
    help="Input csv file path of binary operations benchmark plot",
    default=f"{current_dir}/benchmark_result_binary.csv",
)
parser.add_argument(
    "--measurement-input",
    type=str,
    help="Input csv file of measurement operations benchmark plot",
    default=f"{current_dir}/benchmark_result_measurement.csv",
)
parser.add_argument("--n_cores", type=int, help="Number of cores", default=64)
parser.add_argument("--output_table_csv", action="store_true", help="Output table in csv format")
parser.add_argument("--output_table_md", action="store_true", help="Output table in markdown format")
args = parser.parse_args()
arg_list = []
benchmark_list = []
input_list = []
if args.unary:
    arg_list.append("unary")
    benchmark_list.append("rotation_op_time")
    input_list.append(args.unary_input)
if args.binary:
    arg_list.append("binary")
    benchmark_list.append("bs_op_time")
    input_list.append(args.binary_input)
if args.measurement:
    arg_list.append("measurement")
    benchmark_list.append("measure_op_time")
    input_list.append(args.measurement_input)
if not benchmark_list:
    arg_list = ["unary", "binary", "measurement"]
    benchmark_list = ["rotation_op_time", "bs_op_time", "measure_op_time"]
    input_list = [args.unary_input, args.binary_input, args.measurement_input]

for arg, benchmark, input_csv in zip(arg_list, benchmark_list, input_list, strict=True):
    df_all = pd.read_csv(input_csv)
    # Skim the data to the specific benchmark and number of cores
    df_oi = df_all[(df_all["benchmark"] == benchmark) & (df_all["n_cores"] == args.n_cores)]
    # Slim the dataframe to only include the relevant columns
    df_oi = df_oi[
        [
            "device",
            "n_modes",
            "n_cats",
            "ShotLevelTaskflowOperationLoop",
            "SimulateCPUMultiThread",
            "Execute",
            "SimulateSingleGPU",
        ]
    ]
    df_oi = df_oi.rename(columns={"device": "Device", "n_modes": "Number of modes", "n_cats": "Number of peaks"})
    df_oi["Number of peaks"] = df_oi["Number of peaks"].apply(lambda v: 4**v)

    df_oi_cpu = df_oi[df_oi["Device"] == "cpu_peak_level"]
    df_oi_cpu = df_oi_cpu[
        [
            "Device",
            "Number of modes",
            "Number of peaks",
            "ShotLevelTaskflowOperationLoop",
            "SimulateCPUMultiThread",
        ]
    ]
    df_oi_cpu = df_oi_cpu.rename(
        columns={
            "ShotLevelTaskflowOperationLoop": "Circuit execution time [ms]",
            "SimulateCPUMultiThread": "Total execution time [ms]",
        },
    )

    df_oi_gpu = df_oi[df_oi["Device"] == "single_gpu_single_thread"]
    df_oi_gpu = df_oi_gpu[
        [
            "Device",
            "Number of modes",
            "Number of peaks",
            "Execute",
            "SimulateSingleGPU",
        ]
    ]
    df_oi_gpu = df_oi_gpu.rename(
        columns={
            "Execute": "Circuit execution time [ms]",
            "SimulateSingleGPU": "Total execution time [ms]",
        },
    )

    df_oi_cpu_gpu = pd.concat([df_oi_cpu, df_oi_gpu], ignore_index=True)
    if args.output_table_csv:
        df_oi_cpu_gpu.to_csv(f"plot_bar_table_{arg}.csv", index=False, encoding="utf-8")
        print(f"Saved table to plot_bar_table_{arg}.csv")
    if args.output_table_md:
        df_oi_cpu_gpu.to_markdown(f"plot_bar_table_{arg}.md", index=False)
        print(f"Saved table to plot_bar_table_{arg}.md")

    n_modes = df_oi["Number of modes"].unique().tolist()
    n_peaks = df_oi["Number of peaks"].unique().tolist()
    n_modes_peaks = [(m, c) for m in n_modes for c in n_peaks]

    fig, axs = plt.subplots(len(n_modes), len(n_peaks), figsize=(12, 12))
    categories = ["CPU", "GPU"]

    for ax, (_n_modes, _n_peaks) in zip(axs.flatten(), n_modes_peaks, strict=False):
        data = {}
        condition = (df_oi_cpu["Number of modes"] == _n_modes) & (df_oi_cpu["Number of peaks"] == _n_peaks)
        cpu_exe_selected = df_oi_cpu.loc[condition, ["Circuit execution time [ms]"]]
        cpu_tot_selected = df_oi_cpu.loc[condition, ["Total execution time [ms]"]]
        condition = (df_oi_gpu["Number of modes"] == _n_modes) & (df_oi_gpu["Number of peaks"] == _n_peaks)
        gpu_exe_selected = df_oi_gpu.loc[condition, ["Circuit execution time [ms]"]]
        gpu_tot_selected = df_oi_gpu.loc[condition, ["Total execution time [ms]"]]

        # Extract execution and total times for CPU and GPU. If the selection is empty, assign 0.
        cpu_exe_time = 0 if cpu_exe_selected.empty else float(cpu_exe_selected.iloc[0].item())
        cpu_tot_time = 0 if cpu_tot_selected.empty else float(cpu_tot_selected.iloc[0].item())
        gpu_exe_time = 0 if gpu_exe_selected.empty else float(gpu_exe_selected.iloc[0].item())
        gpu_tot_time = 0 if gpu_tot_selected.empty else float(gpu_tot_selected.iloc[0].item())

        data["CPU"] = (cpu_tot_time, cpu_exe_time)
        data["GPU"] = (gpu_tot_time, gpu_exe_time)

        x = np.arange(len(categories))
        width = 0.6

        # Large value bar
        outer_vals = [float(data[cat][0]) for cat in categories]
        bars_outer = ax.bar(x, outer_vals, width, color="lightblue", edgecolor="black", linewidth=1)
        # Add hatching to the bars representing zero values for better visualization
        for i, val in enumerate(outer_vals):
            if val == 0:
                ax.bar(x[i], ax.get_ylim()[1], width, color="white", hatch="//")

        # Small value bar (inner, dotted border)
        inner_vals = [float(data[cat][1]) for cat in categories]
        bars_inner = ax.bar(x, inner_vals, width * 0.8, color="none", edgecolor="red", linewidth=2)
        for rect in bars_inner:
            rect.set_linestyle(":")

        # Display large values above the top of the large value bars
        for rect, val in zip(bars_outer, outer_vals, strict=False):
            if val != 0:
                height = rect.get_height()
                ax.text(rect.get_x() + rect.get_width() / 2, height, f"{val:.1f}", ha="center", va="bottom")
        # Display small values in red inside the top of the small value bars
        for rect, val in zip(bars_inner, inner_vals, strict=False):
            if val != 0:
                height = rect.get_height()
                ax.text(
                    rect.get_x() + rect.get_width() / 2,
                    height,
                    f"{val:.1f}",
                    ha="center",
                    va="top",
                    color="red",
                )

        ax.set_xticks(x)
        ax.set_xticklabels(categories)
        ax.set_ylabel("Time [ms]")
        # Set the upper limit of the y-axis to match the maximum bar value
        ax.set_ylim(0, max(outer_vals) * 1.15 if max(outer_vals) != 0 else 1)

        # Adjust hatching based on the new y-axis range
        for i, val in enumerate(outer_vals):
            if val == 0:
                ax.bar(x[i], ax.get_ylim()[1], width, color="none", hatch="//")

    plt.tight_layout()

    # Add row labels to the left side of each row
    for i, n in enumerate(n_modes):
        pos = axs[i, 0].get_position()
        fig.text(
            pos.x0 - 0.06,
            pos.y0 + pos.height / 2,
            f"{n} modes",
            va="center",
            ha="right",
            fontsize=12,
            rotation=90,
        )
    # Add column labels at the top of each column
    for i, n in enumerate(n_peaks):
        pos = axs[0, i].get_position()
        fig.text(
            pos.x0 + pos.width / 2,
            pos.y1 + 0.01,
            "1 peak" if n == 1 else f"{n} peak",
            ha="center",
            va="bottom",
            fontsize=12,
        )

    # Place a common legend at the top, adjusting with bbox_to_anchor to avoid overlapping the plot
    legend_elements = [
        Line2D(
            [0],
            [0],
            marker="s",
            color="black",
            markerfacecolor="lightblue",
            markersize=10,
            label="Total execution time",
        ),
        Line2D(
            [0],
            [0],
            marker="s",
            color="red",
            markerfacecolor="none",
            markersize=10,
            label="Circuit execution time",
            linestyle=":",
        ),
    ]
    fig.legend(handles=legend_elements, loc="upper left", bbox_to_anchor=(0.0, 1.075), fontsize=12)

    # Save as SVG
    plt.savefig(f"plot_bar_{arg}.svg", bbox_inches="tight")
    print(f"Plot saved as plot_bar_{arg}.svg")
