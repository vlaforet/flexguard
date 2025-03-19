import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class LevelDBExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads = [
            1,
            2,
            4,
            8,
            16,
            32,
            48,
            64,
            72,
            102,
            104,
            106,
            128,
            192,
            256,
            448,
            512,
        ]

        for lock in locks:
            for t in threads:
                # if t >= 104 and lock in ["mcs", "mcstp", "malthusian"]:
                #    continue

                self.tests.append(
                    {
                        "name": f"LevelDB with {lock} lock and with {t} threads",
                        "benchmark": {
                            "id": "leveldb",
                            "args": {
                                "lock": lock,
                                "threads": t,
                                "time_ms": 30000,
                                "init_db": True,
                                "benchmarks": ["readrandom"],
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        results_ylim = results[~results["lock"].isin(["mcs"])]

        for col in [col for col in results.columns if col.startswith("latency_")]:
            bench_name = col.removeprefix("latency_")

            plt.figure(figsize=(10, 6))
            ax = sns.lineplot(
                data=results_ylim,
                x="threads",
                y=col,
                hue="lock",
                style="lock",
                markers=True,
            )
            ymin, ymax = ax.get_ylim()
            ax.remove()

            sns.lineplot()

            ax2 = sns.lineplot(
                data=results,
                x="threads",
                y=col,
                hue="lock",
                style="lock",
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
