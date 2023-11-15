#! /usr/bin/python3
import matplotlib.pyplot as plt
import argparse
import os

parser = argparse.ArgumentParser(description="Chart data from scheduling benchmark")
parser.add_argument(
    "-i",
    dest="input_folder",
    type=str,
    default="",
    help='Input folder for CSVs (default="")',
)
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
    default=["Futex", "Atomic_CLH", "Hybridlock"],
    help='Locks to show (default=["Futex", "Atomic_CLH", "Hybridlock"])',
)
parser.add_argument(
    "-o",
    dest="output_file",
    type=str,
    default="out.png",
    help='Locks to show (default="out.png")',
)
parser.add_argument(
    "--increasing-only",
    dest="increasing_only",
    action="store_true",
    help="Only show the increasing thread count part (default=False)",
)
args = parser.parse_args()


def load(filename):
    thread_count, ids, throughput = [], [], []
    last_thread_count = 0
    with open(f"{filename}", "r") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 3:
                if args.increasing_only and last_thread_count >= int(cols[1]):
                    break
                last_thread_count = int(cols[1])

                ids.append(int(cols[0]))
                thread_count.append(int(cols[1]))
                throughput.append(float(cols[2]))
    return (thread_count, ids, throughput)


def set_ticks(filename, ax):
    thread_count, ids, _ = load(os.path.join(args.input_folder, f"{filename}.csv"))

    ticks, labels = [], []
    for i in range(len(ids)):
        if thread_count[i] % 5 == 0 and i % 5 == 4:
            ticks.append(ids[i])
            labels.append(thread_count[i])
    ax.set_xticks(ticks)
    ax.set_xticklabels(labels)


def plot(filename, label, ax):
    thread_count, ids, values = load(os.path.join(args.input_folder, f"{filename}.csv"))

    if args.increasing_only:
        ax.plot(thread_count, values, label=label)
    else:
        ax.plot(ids, values, label=label)


fig, ax = plt.subplots()
ax.set_xlabel("Thread Count (40 cores machine)")
ax.set_ylabel("Critical Section time (ms)")
ax.set_yscale("log")

if not args.increasing_only:
    set_ticks(f"{args.lock[0].lower()}_c{args.contention[0]}_t{args.cache_line[0]}", ax)

for contention in args.contention:
    sc = "s" if contention > 1 else ""

    for cl in args.cache_line:
        scl = "s" if cl > 1 else ""

        for lock in args.lock:
            lock_name = lock.replace("_", " ")
            plot(
                f"{lock.lower()}_c{contention}_t{cl}",
                f"{lock_name}, {cl} line{scl}, {contention} cycle{sc}",
                ax,
            )

fig.legend(loc="right", bbox_to_anchor=(0.77, 0.245))
plt.savefig(args.output_file, dpi=600, bbox_inches="tight")
