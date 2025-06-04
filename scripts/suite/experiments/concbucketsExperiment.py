import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class ConcBucketsExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads = [1, 2, 4, 8, 16, 32, 48, 50, 52, 54, 64, 72, 96, 128, 192, 256]

        for lock in locks:
            for thread in threads:
                self.tests.append(
                    {
                        "name": f"Buckets using {lock} lock and concurrent workload with {thread} threads",
                        "benchmark": {
                            "id": "buckets",
                            "args": {
                                "lock": lock,
                                "duration": 10000,
                                "num-threads": 52,
                                "buckets": 100,
                                "max-value": 100000,
                                "offset-changes": 40,
                            },
                        },
                        "concurrent": {
                            "id": "scheduling",
                            "args": {
                                "lock": "mcstas",
                                "base-threads": thread,
                                "num-threads": thread,
                                "step-duration": 15000,
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        plt.figure(figsize=(10, 6))
        sns.lineplot(
            data=results,
            x="concurrent_num-threads",
            y="throughput",
            hue="lock",
            style="lock",
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

        output_path = os.path.join(exp_dir, "concbuckets.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
