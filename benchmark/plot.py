# ruff: noqa: PLR2004


import csv
import sys
from dataclasses import dataclass

import matplotlib.pyplot as plt

csv_path = sys.argv[1]
benchmark = sys.argv[2]
section = "ShotLevelTaskflowOperationLoop"

with open(csv_path, newline="", encoding="utf-8") as f:
    csv_list = list(csv.DictReader(f))


@dataclass
class Series:
    n_modes: int
    n_cats: int
    color: str = ""
    marker: str = ""
    label: str = ""

    def __post_init__(self) -> None:
        if self.n_modes == 10:
            self.color = "red"
        elif self.n_modes == 100:
            self.color = "blue"
        elif self.n_modes == 1000:
            self.color = "green"

        if self.n_cats == 0:
            self.marker = "o"
        elif self.n_cats == 2:
            self.marker = "s"
        elif self.n_cats == 5:
            self.marker = "^"

        self.label = f"{self.n_modes} modes {4**self.n_cats} peaks"


series_list = [
    Series(n_modes=10, n_cats=0),
    Series(n_modes=10, n_cats=2),
    Series(n_modes=10, n_cats=5),
    Series(n_modes=100, n_cats=0),
    Series(n_modes=100, n_cats=2),
    Series(n_modes=100, n_cats=5),
    Series(n_modes=1000, n_cats=0),
    Series(n_modes=1000, n_cats=2),
    Series(n_modes=1000, n_cats=5),
]

fig, ax = plt.subplots(figsize=(6, 10))

for series in series_list:
    n_cores = [
        int(row["n_cores"])
        for row in csv_list
        if row["benchmark"] == benchmark
        and int(row["n_modes"]) == series.n_modes
        and int(row["n_cats"]) == series.n_cats
    ]
    legends = [
        f"{row['n_modes']} modes {row['n_cats']} cats"
        for row in csv_list
        if row["benchmark"] == benchmark
        and int(row["n_modes"]) == series.n_modes
        and int(row["n_cats"]) == series.n_cats
    ]
    data = [
        float(row[section])
        for row in csv_list
        if row["benchmark"] == benchmark
        and int(row["n_modes"]) == series.n_modes
        and int(row["n_cats"]) == series.n_cats
    ]
    ax.scatter(n_cores, data, color=series.color, marker=series.marker, label=series.label)

ax.set_xlabel("Number of cores")
ax.set_ylabel(f"{section} time [ms]")
ax.set_xscale("log")
ax.set_yscale("log")
ax.legend(loc="upper left", bbox_to_anchor=(1.05, 1))
plt.savefig("plot.png", bbox_inches="tight")
