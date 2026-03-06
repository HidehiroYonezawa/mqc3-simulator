# ruff: noqa: PLR2004


import csv
import sys
from dataclasses import dataclass

import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

csv_path = sys.argv[1:]
benchmark = "measure_op_time"
section = "ShotLevelTaskflowOperationLoop"
n_modes = 100
n_cats = 5

csv_lists = []
for path in csv_path:
    with open(path, newline="", encoding="utf-8") as f:
        csv_lists.append(list(csv.DictReader(f)))


@dataclass
class Series:
    color: str
    marker: str
    label: str


series_list = [
    Series(color="black", marker="o", label="Before update"),
    Series(color="blue", marker="o", label="After parallelization"),
    Series(color="red", marker="o", label="After post-selection optimization"),
]

fig = plt.figure(figsize=(8, 8))
gs = GridSpec(3, 1, height_ratios=[3, 1, 1])
ax1 = fig.add_subplot(gs[0])
ax2 = fig.add_subplot(gs[1], sharex=ax1)
ax3 = fig.add_subplot(gs[2], sharex=ax1)

dataset = [
    {
        int(row["n_cores"]): float(row[section])
        for row in csv_list
        if row["benchmark"] == benchmark and int(row["n_modes"]) == n_modes and int(row["n_cats"]) == n_cats
    }
    for csv_list in csv_lists
]

for i, (csv_list, data, series) in enumerate(zip(csv_lists, dataset, series_list, strict=False)):
    n_cores = [
        int(row["n_cores"])
        for row in csv_list
        if row["benchmark"] == benchmark and int(row["n_modes"]) == n_modes and int(row["n_cats"]) == n_cats
    ]
    legends = [
        f"{row['n_modes']} modes {row['n_cats']} cats"
        for row in csv_list
        if row["benchmark"] == benchmark and int(row["n_modes"]) == n_modes and int(row["n_cats"]) == n_cats
    ]
    ax1.scatter(n_cores, list(data.values()), color=series.color, marker=series.marker, label=series.label)

    if i == 0:
        continue

    n_cores_slim = []
    diff = []
    rel_diff = []
    for n_c0, val0 in dataset[0].items():
        if n_c0 in data:
            val = data[n_c0]
            n_cores_slim.append(n_c0)
            diff.append(val - val0)
            rel_diff.append((val - val0) / val0)
    ax2.scatter(n_cores_slim, diff, color=series.color, marker=series.marker, label=series.label)
    ax3.scatter(n_cores_slim, rel_diff, color=series.color, marker=series.marker, label=series.label)

ax3.set_xlabel("Number of cores")

ax1.set_ylabel(f"{section} time [ms]")
ax2.set_ylabel("Difference [ms]")
ax3.set_ylabel("Relative diff.")
for ax in [ax1, ax2, ax3]:
    ax.set_xscale("log")
    ax.grid(True)
ax1.set_yscale("log")

x_min, x_max = ax2.get_xlim()
ax1.legend(loc="upper left", bbox_to_anchor=(1.05, 1))
plt.subplots_adjust(hspace=0)
plt.savefig("plot.png", bbox_inches="tight")
