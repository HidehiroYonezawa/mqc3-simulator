# ruff: noqa: S404, S603, S607, SIM105, T201

import argparse
import subprocess
from pathlib import Path

from utility import mask_str

parser = argparse.ArgumentParser(description="Commandline options for benchmarking")

parser.add_argument("--unary", action="store_true", help="Execute unary operations benchmark")
parser.add_argument("--binary", action="store_true", help="Execute binary operations benchmark")
parser.add_argument("--measurement", action="store_true", help="Execute measurement operations benchmark")

parser.add_argument("--cpu", action="store_true", help="Execute benchmark on cpu backend")
parser.add_argument(
    "--cpu_shot_level",
    action="store_true",
    help="Execute benchmark on shot level parallel cpu backend",
)
parser.add_argument(
    "--cpu_peak_level",
    action="store_true",
    help="Execute benchmark on peak level parallel cpu backend",
)
parser.add_argument("--single_gpu", action="store_true", help="Execute benchmark on single gpu backend")
parser.add_argument("--single_gpu_single_thread", action="store_true", help="Execute benchmark on single gpu backend")
parser.add_argument("--multi_gpu", action="store_true", help="Execute benchmark on multi gpu backend")

parser.add_argument("--n_shots", type=int, help="Number of shots", default=1)
parser.add_argument("--n_warmup", type=int, help="Number of warmup", default=0)
parser.add_argument("--n_trial", type=int, help="Number of trials", default=1)
parser.add_argument("--n_cores", type=int, help="Number of cores", default=64)
parser.add_argument("--timeout", type=int, help="Timeout [sec]", default=60)

args = parser.parse_args()

default_operation_list = [("unary", "rotation_op_time"), ("binary", "bs_op_time"), ("measurement", "measure_op_time")]
operation_list = []
for operation, cmd in default_operation_list:
    if hasattr(args, operation) and getattr(args, operation):
        operation_list.append((operation, cmd))
if len(operation_list) == 0:
    operation_list = default_operation_list

default_device_list = ["cpu", "cpu_shot_level", "cpu_peak_level", "single_gpu", "single_gpu_single_thread", "multi_gpu"]
device_list = [device for device in default_device_list if hasattr(args, device) and getattr(args, device)]
if len(device_list) == 0:
    device_list = default_device_list

print(f"operation = {operation_list}")
print(f"device    = {device_list}")
print(f"n_shots   = {args.n_shots}")
print(f"n_warmup  = {args.n_warmup}")
print(f"n_trial   = {args.n_trial}")
print(f"n_cores   = {args.n_cores}")
print(f"timeout   = {args.timeout}")

current_dir = Path.cwd()
for operation, cmd in operation_list:
    output_file_name = f"{current_dir}/benchmark_result_{operation}.csv"
    print(f"Writing to {output_file_name}")
    subprocess.run(["./build/benchmark/table_header", output_file_name], check=False)

    for device in device_list:
        for n_cores in [args.n_cores]:
            for n_modes in [10, 500, 2000]:
                for n_cats in [0, 2, 5]:
                    # Skip too much time consuming case.
                    if n_modes >= 2000 and n_cats >= 5:  # noqa: PLR2004
                        continue

                    try:
                        subprocess.run(
                            [
                                "taskset",
                                f"{mask_str(n_cores)}",
                                f"./build/benchmark/{cmd}",
                                f"{device}",
                                f"{n_cores}",
                                f"{n_modes}",
                                f"{n_cats}",
                                output_file_name,
                                f"{args.n_shots}",
                                f"{args.n_warmup}",
                                f"{args.n_trial}",
                            ],
                            timeout=args.timeout,
                            check=False,
                            # On timeout, skip to the next execution without error
                        )
                    except subprocess.TimeoutExpired:
                        pass
