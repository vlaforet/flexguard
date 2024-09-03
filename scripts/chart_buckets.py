#! /usr/bin/python3
import matplotlib.pyplot as plt
import argparse
import os
import numpy as np

parser = argparse.ArgumentParser(description="Chart data from buckets benchmark")
parser.add_argument(
    "-i",
    dest="input_file",
    type=str,
    default="results/buckets.csv",
    help='Input CSV file (default="results/buckets.csv")',
)
parser.add_argument(
    "--labels",
    dest="locks_label",
    type=str,
    nargs="+",
    default=None,
    help="Labels for locks. Should be ordered in the same way as the locks (default=None)",
)
parser.add_argument(
    "-l",
    dest="lock",
    type=str,
    nargs="+",
    default=None,
    help="Locks to show (default=None (show all))",
)
parser.add_argument(
    "-t",
    dest="thread",
    type=int,
    nargs="+",
    default=None,
    help="Threads to show (default=None (show all))",
)
parser.add_argument(
    "-o",
    dest="output_file",
    type=str,
    default="out.png",
    help='Output file (default="out.png")',
)
args = parser.parse_args()


def load(filename):
    res = []
    with open(f"{filename}", "r") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if (
                len(cols) >= 3
                and cols[0] != "Lock"
                and (args.lock == None or cols[0].lower() in args.lock)
                and (args.thread == None or int(cols[1]) in args.thread)
            ):
                res.append((cols[0], int(cols[1]), float(cols[2])))

    return np.array(
        res, dtype=[("lock", "U20"), ("threads", "i4"), ("throughput", "f8")]
    )


fig, ax = plt.subplots(layout="constrained")
# ax.set_yscale("log")

csv = load(args.input_file)
csv = np.sort(csv, order="lock")
locks = np.unique(csv["lock"])
threads = np.unique(csv["threads"])

width = 1
x = np.arange(len(locks)) * (width * (len(threads) + 1))
ax.set_xticks(
    x + (len(threads) - 1) / 2 * width,
    locks if args.locks_label == None else args.locks_label,
)

for key, threads in enumerate(threads):
    values = csv[csv["threads"] == threads]["throughput"]

    ax.bar(x + width * key, values, width, label=f"{threads} threads", color=f"C{key}")

    ax.hlines(
        [np.max(values)] * len(locks),
        x + width * (key - 0.5),
        x + width * (key + 0.5),
        colors=f"C{key}",  # type: ignore
        linestyle="dashed",
        linewidths=1,
    )


ax.set_ylabel("Throughput (CS/s)")
ax.legend(loc="upper center", ncols=4, bbox_to_anchor=(0.4, -0.05))
plt.savefig(args.output_file, dpi=600, bbox_inches="tight")
