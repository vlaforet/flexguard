#! /usr/bin/python3

import matplotlib.pyplot as plt
import numpy as np
import sys


def load(filename):
    thread_count, time, throughput = [], [], []
    with open(filename) as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 3:
                thread_count.append(int(cols[0]))
                time.append(float(cols[1]))
                throughput.append(float(cols[2]))
    return (thread_count, time, throughput)


thread_count, time, hybrid = load("hybrid.csv")
_, time_futex, futex = load("futex.csv")
_, time_hybrid_spin, hybrid_spin = load("hybrid_spin.csv")

if len(time) == 0:
    sys.exit(1)

fig, ax = plt.subplots()
ax.step(time, thread_count, label="Thread Count", color="red")
ax.set_ylabel("Thread Count")
ax.set_xlabel("Elapsed time")

ax2 = ax.twinx()
ax2.plot(time, hybrid, label="Hybrid", color="blue")
ax2.plot(time_futex, futex, label="Futex", color="green")
ax2.plot(time_hybrid_spin, hybrid_spin, label="Hybrid spin", color="orange")
ax2.set_yscale("log")
ax2.set_ylabel("Throughput")

fig.legend()
plt.savefig("out.png")
