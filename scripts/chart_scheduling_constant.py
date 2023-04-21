#! /usr/bin/python3
import matplotlib.pyplot as plt
import numpy as np
import sys


def load(filename):
    thread_count, time, throughput = [], [], []
    with open(f"./tmp/{filename}") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 3:
                thread_count.append(int(cols[0]))
                time.append(float(cols[1]))
                throughput.append(float(cols[2]))
    return (thread_count, time, throughput)


def load_and_plot(filename, axis):
    _, t, data = load(f"{filename}.csv")
    axis.plot(t, data, label=filename)


fig, ax = plt.subplots()
# ax.set_yscale("log")
ax.set_ylabel("Critical Section time (ms)")
ax.set_xlabel("Elapsed time")

for fn in ["futex", "hybrid_futex", "hybrid_mcs", "mcs"]:
    load_and_plot(fn, ax)

fig.legend()
plt.savefig("out.png", dpi=600, bbox_inches="tight")
