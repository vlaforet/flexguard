import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class FairnessExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads = [52, 156]

        cycles = [
            0,
            10,
            100,
            1000,
            10000,
            100000,
            1000000,
            10000000,
            100000000,
            1000000000,
            10000000000,
            100000000000,
        ]

        for t in threads:
            for cycle in cycles:
                for lock in locks:
                    self.tests.append(
                        {
                            "name": f"Short-term fairness buckets with {lock} lock and {cycle} cycles and {t} threads",
                            "benchmark": {
                                "id": "buckets",
                                "args": {
                                    "lock": lock,
                                    "duration": 1000,
                                    "num-threads": t,
                                    "buckets": 1,
                                    "max-value": 100000,
                                    "offset-changes": 1,
                                    "non-critical-cycles": cycle,
                                    "pin-threads": 1,
                                },
                            },
                        }
                    )
                    self.tests.append(
                        {
                            "name": f"Long-term fairness buckets with {lock} lock and {cycle} cycles and {t} threads",
                            "benchmark": {
                                "id": "buckets",
                                "args": {
                                    "lock": lock,
                                    "duration": 10000,
                                    "num-threads": t,
                                    "buckets": 1,
                                    "max-value": 100000,
                                    "offset-changes": 1,
                                    "non-critical-cycles": cycle,
                                    "pin-threads": 1,
                                },
                            },
                        },
                    )

    def report(self, results, exp_dir):
        throughput_columns = [
            col for col in results.columns if col.startswith("throughput_t")
        ]

        avg_results = (
            results.groupby(["lock", "duration", "non-critical-cycles"])[
                throughput_columns
            ]
            .mean()
            .reset_index()
        )

        avg_results["CV"] = (
            avg_results[throughput_columns].std(axis=1)
            / avg_results[throughput_columns].mean(axis=1)
            * 100
        )

        longterm = avg_results[avg_results["duration"] == 10000]
        shorterm = avg_results[avg_results["duration"] == 1000]

        fig, axes = plt.subplots(2, 1, figsize=(10, 6), sharex=True)
        plt.xscale("symlog")

        sns.lineplot(
            data=shorterm,
            x="non-critical-cycles",
            y="CV",
            hue="lock",
            style="lock",
            markers=True,
            ax=axes[0],
        )
        axes[0].set_title(
            "(a) Long-term fairness (Coefficient of Variation, 10 seconds) (Lower is better)"
        )
        axes[0].set_xlabel("")
        axes[0].set_ylabel("Coefficient of Variation (%)")
        axes[0].grid(True)
        axes[0].legend().remove()

        sns.lineplot(
            data=longterm,
            x="non-critical-cycles",
            y="CV",
            hue="lock",
            style="lock",
            markers=True,
            ax=axes[1],
        )
        axes[1].set_title(
            "(b) Short-term fairness (Coefficient of Variation, 1 second) (Lower is better)"
        )
        axes[1].set_xlabel("Time between critical sections (cycles)")
        axes[1].set_ylabel("Coefficient of Variation (%)")
        axes[1].grid(True)
        axes[1].legend(
            loc="lower center",
            bbox_to_anchor=(0.46, -0.35),
            borderaxespad=0,
            frameon=False,
            ncol=len(axes[0].get_legend_handles_labels()[1]),
            columnspacing=0.8,
        )

        output_path = os.path.join(exp_dir, "fairness.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
