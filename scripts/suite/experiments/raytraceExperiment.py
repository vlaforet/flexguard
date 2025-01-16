import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class RaytraceExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads = [1, 2] + [i for i in range(10, 128, 10)] + [128]

        for label, lock in locks.items():
            for t in threads:
                if lock == "mcs" and t > 100:
                    continue
                self.tests.append(
                    {
                        "name": f"Raytrace with {label} lock and {t} threads",
                        "label": label,
                        "benchmark": {
                            "id": "raytrace",
                            "args": {
                                "lock": lock,
                                "threads": t,
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        results_ylim = results[~results["lock"].isin(["mcs"])]

        plt.figure(figsize=(10, 6))
        plt.xlim(20, 128)
        sns.lineplot(
            data=results,
            x="threads",
            y="time",
            hue="label",
            style="label",
            markers=True,
        )

        plt.title(f"SPLASH2x Raytrace Benchmark (Lower is better)")
        plt.xlabel("Threads")
        plt.ylabel("Execution time (micros)")
        plt.grid(True)
        # plt.yscale("log")

        output_path = os.path.join(exp_dir, "raytrace.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
