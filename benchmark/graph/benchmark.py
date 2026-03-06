"""Benchmarking script for the graph simulator."""

import contextlib
import os
import subprocess  # noqa: S404
import sys
from pathlib import Path

from google.protobuf import text_format
from mqc3.pb.convert_graph import construct_proto_from_graph_repr

parent_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, parent_dir)

from utility import mask_str

from benchmark.graph.graph_repr import create_graph_repr

build_dir = Path("./build/benchmark/graph")


def run_simulation(
    n_cores: int,
    n_local_macronodes: int,
    n_steps: int,
    n_shots: int,
    graph_repr_path: str,
    output_file_name: str,
) -> None:
    """Run a simulation with the given parameters.

    Args:
       n_cores: Number of cores to use
       n_local_macronodes: Number of local macronodes
       n_steps: Number of steps
       n_shots: Number of shots
       graph_repr_path: Path to the graph representation proto
       output_file_name: Output file name
    """
    graph_repr = create_graph_repr(n_local_macronodes, n_steps)
    pb_graph_repr = construct_proto_from_graph_repr(graph_repr)
    text_data = text_format.MessageToString(pb_graph_repr)
    Path(graph_repr_path).write_text(text_data, encoding="utf-8")
    assert Path(graph_repr_path).is_file()

    with contextlib.suppress(subprocess.TimeoutExpired):
        subprocess.run(  # noqa: S603
            [  # noqa: S607
                "taskset",
                f"{mask_str(n_cores)}",
                f"{build_dir}/graph_simulator_time",
                graph_repr_path,
                output_file_name,
                "cpu",
                f"{n_cores}",
                f"{n_local_macronodes}",
                f"{n_steps}",
                f"{n_shots}",
            ],
            check=False,
        )


if __name__ == "__main__":
    output_file_name = f"{Path.cwd()}/graph_benchmark_result.csv"
    subprocess.run([f"{build_dir}/graph_table_header", output_file_name], check=False)  # noqa: S603
    device = "cpu"
    n_shots = 1
    for n_cores in [128]:
        for n_local_macronodes in [101, 500]:
            for n_steps in [128, 256, 512, 1024]:
                run_simulation(
                    n_cores=n_cores,
                    n_local_macronodes=n_local_macronodes,
                    n_steps=n_steps,
                    n_shots=n_shots,
                    graph_repr_path="graph_benchmark_repr.txt",
                    output_file_name=output_file_name,
                )
