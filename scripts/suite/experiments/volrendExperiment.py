import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class VolrendExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        locks = {
            "POSIX": "mutex",
            "Stock": "stock",
            "MCS": "mcs",
            "BHL": "hybridv2",
        }

        threads = [1, 2] + [i for i in range(10, 182, 10)]

        for label, lock in locks.items():
            for t in threads:
                if lock == "mcs" and t > 100:
                    continue
                self.tests.append(
                    {
                        "name": f"Volrend with {label} lock and {t} threads",
                        "label": label,
                        "benchmark": {
                            "id": "volrend",
                            "args": {
                                "lock": lock,
                                "threads": t,
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        results_ylim = results[
            ~results["lock"].isin(["mcs", "hybridv2nextwaiterdetection"])
        ]

        for col in [col for col in results.columns if col.startswith("latency_")]:
            bench_name = col.removeprefix("latency_")

            plt.figure(figsize=(10, 6))
            ax = sns.lineplot(
                data=results_ylim,
                x="threads",
                y=col,
                hue="label",
                style="label",
                markers=True,
            )
            ymin, ymax = ax.get_ylim()
            ax.remove()

            sns.lineplot()

            ax2 = sns.lineplot(
                data=results,
                x="threads",
                y=col,
                hue="label",
                style="label",
                markers=True,
            )
            ax2.set_ylim(ymin, ymax)

            plt.xlabel("Threads")
            plt.ylabel("Latency (micros/op)")
            plt.title(f"LevelDB {bench_name} Benchmark (Lower is better)")
            plt.legend(title="Lock")
            plt.grid(True)
            plt.ylim(bottom=0)

            output_path = os.path.join(exp_dir, f"{bench_name}.png")
            plt.savefig(output_path, dpi=600, bbox_inches="tight")
            print(f"Wrote plot to {output_path}")
