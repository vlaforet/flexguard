import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class SchedulingExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "BPF Hybrid Lock": "hybridv2",
            "BPF Hybrid Lock No Next Waiter Sleeping Detection": "hybridv2_no_next_waiter_detection",
            "MCS": "mcs",
            "Pthread Mutex": "mutex",
        }

        for label, lock in locks.items():
            self.tests.append(
                {
                    "benchmark": "scheduling",
                    "name": f"Scheduling using {label} lock",
                    "label": label,
                    "kwargs": {
                        "lock": lock,
                        "base-threads": 0,
                        "num-threads": 180,
                        "step-duration": 5000,
                        "cache-lines": 5,
                        "thread-step": 10,
                        "increasing-only": 1,
                    },
                }
            )

    def report(self, results, exp_dir):
        plt.figure(figsize=(10, 6))
        sns.lineplot(
            data=results,
            x="threads",
            y="throughput",
            hue="label",
            style="label",
            markers=True,
        )

        plt.title("Single-lock Microbenchmark (Higher is better)")
        plt.xlabel("Threads")
        plt.ylabel("Throughput (OPs/s)")
        plt.grid(True)

        output_path = os.path.join(exp_dir, "scheduling.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
