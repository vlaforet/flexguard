#! /usr/bin/python3
import matplotlib.pyplot as plt
import argparse
import os

parser = argparse.ArgumentParser(
    description="Chart data from contention scheduling benchmark"
)
parser.add_argument(
    "-i",
    dest="input_folder",
    type=str,
    default="",
    help='Input folder for CSVs (default="")',
)
parser.add_argument(
    "-t",
    dest="cache_line",
    type=int,
    nargs="+",
    default=[1],
    help="Cache lines to show (default=[1])",
)
parser.add_argument(
    "-n",
    dest="thread_count",
    type=int,
    nargs="+",
    default=[30, 40, 50],
    help="Thread counts to show (default=[30, 40, 50])",
)
parser.add_argument(
    "-l",
    dest="lock",
    type=str,
    nargs="+",
    default=["Futex", "Atomic_CLH", "Hybridlock"],
    help='Locks to show, case will be displayed as is (default=["Futex", "Atomic_CLH", "Hybridlock"])',
)
parser.add_argument(
    "-o",
    dest="output_file",
    type=str,
    default="out.png",
    help='Locks to show (default="out.png")',
)
args = parser.parse_args()


def load(filename):
    contention, throughput = [], []
    with open(f"{filename}", "r") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 2:
                contention.append(int(cols[0]))
                throughput.append(float(cols[1]))
    return (contention, throughput)


def plot(filename, label, ax):
    contention, values = load(
        os.path.join(args.input_folder, f"contention_{filename}.csv")
    )
    ax.plot(contention, values, label=label)


fig, ax = plt.subplots()
ax.set_xlabel("Contention (Cycles between critical sections)")
ax.set_ylabel("Critical Section time (ms)")
ax.set_yscale("log")
ax.set_xscale("log")

for thread_count in args.thread_count:
    for cl in args.cache_line:
        scl = "s" if cl > 1 else ""

        for lock in args.lock:
            lock_name = lock.replace("_", " ")
            plot(
                f"{lock.lower()}_t{cl}_n{thread_count}",
                f"{lock_name}, {cl} line{scl} {thread_count} threads",
                ax,
            )

fig.legend()
plt.savefig(args.output_file, dpi=600, bbox_inches="tight")
