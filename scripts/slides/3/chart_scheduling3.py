#! /usr/bin/python3
import matplotlib.pyplot as plt
import numpy as np
import sys


def load(filename):
    thread_count, time, throughput = [], [], []
    with open(f"{filename}") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 3:
                thread_count.append(int(cols[0]))
                time.append(float(cols[1]))
                throughput.append(float(cols[2]))
    return (thread_count, time, throughput)


thread_count, time, futex = load("futex.csv")

if len(time) == 0:
    sys.exit(1)

fig, ax = plt.subplots()
ax.step(time, thread_count, label="Thread Count", color="red")
ax.set_ylabel("Thread Count")
ax.set_xlabel("Elapsed time")
ax.plot([0, max(time)], [40, 40], label="Core Count", color="purple")

ax2 = ax.twinx()
ax2.plot(time, futex, label="CS Execution Time")
ax2.set_yscale("log")
ax2.set_ylabel("CS Execution time (ms)")

fig.legend()
plt.savefig("out.png", dpi=600, bbox_inches="tight")
