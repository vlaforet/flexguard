#! /usr/bin/python3
import matplotlib.pyplot as plt
import numpy as np
import sys


def load(filename):
    thread_count, time, throughput = [], [], []
    with open(f"../tmp/{filename}") as f:
        for line in f:
            cols = line.rstrip().split(",")
            if len(cols) >= 3:
                thread_count.append(int(cols[0]))
                time.append(float(cols[1]))
                throughput.append(float(cols[2]))
    return (thread_count, time, throughput)


thread_count, time, hybrid = load("hybrid.csv")
_, time_hybrid_emulated, hybrid_emulated = load("hybrid_emulated.csv")
_, time_hybrid_spin, hybrid_spin = load("hybrid_spin.csv")
_, time_hybrid_blocking, hybrid_blocking = load("hybrid_blocking.csv")
_, time_mcs, mcs = load("mcs.csv")
_, time_mutex, mutex = load("mutex.csv")
_, time_futex, futex = load("futex.csv")
_, time_spin, spin = load("spin.csv")

if len(time) == 0:
    sys.exit(1)

fig, ax = plt.subplots()
ax.step(time, thread_count, label="Thread Count", color="red")
ax.set_ylabel("Thread Count")
ax.set_xlabel("Elapsed time")

ax2 = ax.twinx()
ax2.plot(time_hybrid_spin, hybrid_spin, label="Hybrid spin", color="orange")
ax2.plot(time_hybrid_blocking, hybrid_blocking, label="Hybrid blocking", color="grey")
ax2.plot(time_spin, spin, label="Spin", color="cyan")
ax2.plot(time_mutex, mutex, label="Mutex", color="magenta")
ax2.plot(time_futex, futex, label="Futex", color="brown")
ax2.plot(time_mcs, mcs, label="MCS", color="purple")
ax2.plot(time_hybrid_emulated, hybrid_emulated, label="Hybrid Emulated", color="green")
ax2.plot(time, hybrid, label="Hybrid", color="blue")
ax2.set_yscale("log")
ax2.set_ylabel("Critical Section time (ms)")

fig.legend()
plt.savefig("../out.png", dpi=600, bbox_inches="tight")
