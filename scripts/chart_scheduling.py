#! /usr/bin/python3
import matplotlib.pyplot as plt
import argparse


def load(filename):
    thread_count, ids, throughput = [], [], []
    with open(f"./{filename}", "r") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 3:
                ids.append(int(cols[0]))
                thread_count.append(int(cols[1]))
                throughput.append(float(cols[2]))
    return (thread_count, ids, throughput)


def set_ticks(filename, ax):
    thread_count, ids, _ = load(f"{filename}.csv")

    ticks, labels = [], []
    for i in range(len(ids)):
        if thread_count[i] % 5 == 0 and i % 5 == 4:
            ticks.append(ids[i])
            labels.append(thread_count[i])
    ax.set_xticks(ticks)
    ax.set_xticklabels(labels)


def plot(filename, label, ax):
    _, ids, values = load(f"{filename}.csv")
    ax.plot(ids, values, label=label)


parser = argparse.ArgumentParser(description="Chart data from scheduling benchmark")
parser.add_argument(
    "-c",
    dest="contention",
    type=int,
    nargs="+",
    default=[1000],
    help="Contention delays to show (default=[1000])",
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
ax.set_xlabel("Thread Count (40 cores machine)")
ax.set_ylabel("Critical Section time (ms)")
ax.set_yscale("log")

set_ticks(f"{args.lock[0]}_c{args.contention[0]}_t{args.cache_line[0]}", ax)

for contention in args.contention:
    sc = "s" if contention > 1 else ""

    for cl in args.cache_line:
        scl = "s" if cl > 1 else ""

        for lock in args.lock:
            lock_name = lock.replace("_", " ").capitalize()
            plot(
                f"{lock}_c{contention}_t{cl}",
                f"{lock_name}, {cl} line{scl}, {contention} cycle{sc}",
                ax,
            )

fig.legend()
plt.savefig(args.output_file, dpi=600, bbox_inches="tight")
