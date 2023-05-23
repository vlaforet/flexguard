#! /usr/bin/python3
import matplotlib.pyplot as plt
import argparse


def load(filename):
    contention, throughput = [], []
    with open(f"./{filename}", "r") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 2:
                contention.append(int(cols[0]))
                throughput.append(float(cols[1]))
    return (contention, throughput)


def plot(filename, label, ax):
    contention, values = load(f"contention_{filename}.csv")
    ax.plot(contention, values, label=label)


parser = argparse.ArgumentParser(
    description="Chart data from contention scheduling benchmark"
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
    default=["futex", "mcs", "hybrid_emulated"],
    help='Locks to show (default=["futex", "mcs", "hybrid_emulated"])',
)
parser.add_argument(
    "-o",
    dest="output_file",
    type=str,
    default="out.png",
    help='Locks to show (default="out.png")',
)
args = parser.parse_args()

fig, ax = plt.subplots()
ax.set_xlabel("Contention (Cycles between critical sections)")
ax.set_ylabel("Critical Section time (ms)")
ax.set_yscale("log")
ax.set_xscale("log")

for thread_count in args.thread_count:
    for cl in args.cache_line:
        scl = "s" if cl > 1 else ""

        for lock in args.lock:
            lock_name = lock.replace("_", " ").capitalize()
            plot(
                f"{lock}_t{cl}_n{thread_count}",
                f"{lock_name}, {cl} line{scl} {thread_count} threads",
                ax,
            )

fig.legend()
plt.savefig(args.output_file, dpi=600, bbox_inches="tight")
