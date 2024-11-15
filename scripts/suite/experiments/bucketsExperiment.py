import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class BucketsExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "BPF Hybrid Lock": "hybridv2",
            "BPF Hybrid Lock No Next Waiter Sleeping Detection": "hybridv2nonextwaiterdetection",
            "MCS": "mcs",
            "Pthread Mutex": "mutex",
        }

        threads = [1, 2] + [x for x in range(10, 190, 20)]

        for label, lock in locks.items():
            for thread in threads:
                self.tests.append(
                    {
                        "benchmark": "buckets",
                        "name": f"Buckets using {label} lock and {thread} threads",
                        "label": label,
                        "kwargs": {
                            "lock": lock,
                            "duration": 10000,
                            "num-threads": thread,
                            "buckets": 100,
                            "max-value": 100000,
                            "offset-changes": 40,
                        },
                    }
                )

    def report(self, results, exp_dir):
        plt.figure(figsize=(10, 6))
        sns.lineplot(
            data=results,
            x="num-threads",
            y="throughput",
            hue="label",
            style="label",
            markers=True,
        )

        plt.title(
            f"{results['buckets'].get(0)} Buckets Hashtable Benchmark (Higher is better)"
        )
        plt.xlabel("Threads")
        plt.ylabel("Throughput (OPs/s)")
        plt.grid(True)
        plt.yscale("log")

        output_path = os.path.join(exp_dir, "buckets.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
