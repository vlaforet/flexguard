import os

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class PauseExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads = [1, 2] + [x for x in range(10, 301, 20)]

        for lock in locks:
            for thread in threads:
                self.tests.append(
                    {
                        "name": f"Pause using {lock} lock and {thread} threads",
                        "benchmark": {
                            "id": "buckets",
                            "args": {
                                "lock": lock,
                                "num-threads": thread,
                                "duration": 2000,
                                "buckets": 1,
                                "max-value": 100000,
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        plt.figure(figsize=(10, 6))

        sns.lineplot(
            data=results,
            x="num-threads",
            y="pauses",
            hue="lock",
            style="lock",
            markers=True,
        )

        plt.title(f"Busy-waiting loops")
        plt.xlabel("Threads")
        plt.ylabel("Busy-waiting loops (OPs)")
        plt.grid(True)

        output_path = os.path.join(exp_dir, "pauses.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
