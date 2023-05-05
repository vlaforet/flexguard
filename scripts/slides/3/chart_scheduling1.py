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


thread_count_futex, _, futex = load("futex1.csv")
thread_count_futex = np.array(thread_count_futex) / 40

fig, ax = plt.subplots()
ax.set_xlabel("Thread count / Core count")

ax.plot(thread_count_futex, futex, label="CS Execution Time")

ax.set_yscale("log")
ax.set_ylabel("CS Execution Time (ms)")

fig.legend()
plt.savefig("out.png", dpi=600, bbox_inches="tight")
