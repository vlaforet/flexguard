import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class ConcBucketsExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "LoadRunner": "hybridv2",
            "MCS": "mcs",
            "Pthread Mutex": "mutex",
        }

        threads = [1] + [x for x in range(5, 300, 5)]

        for label, lock in locks.items():
            for thread in threads:
                self.tests.append(
                    {
                        "benchmark": "buckets",
                        "concurrent": {
                            "benchmark": "scheduling",
                            "kwargs": {
                                "lock": "mcstas",
                                "base-threads": thread,
                                "num-threads": thread,
                                "step-duration": 15000,
                            },
                        },
                        "name": f"Buckets using {label} lock and concurrent workload with {thread} threads",
                        "label": label,
                        "kwargs": {
                            "lock": lock,
                            "duration": 10000,
                            "num-threads": 52,
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
            x="concurrent_num-threads",
            y="throughput",
            hue="label",
            style="label",
            markers=True,
        )

        plt.title(
            f"{results['buckets'].get(0)} Buckets Hashtable Concurrent Workload Benchmark (Higher is better)"
        )
        plt.xlabel(
            f"Concurrent Threads (With {results['num-threads'].get(0)} benchmark threads)"
        )
        plt.ylabel("Throughput (OPs/s)")
        plt.grid(True)
        plt.yscale("log")

        output_path = os.path.join(exp_dir, "concbuckets.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
