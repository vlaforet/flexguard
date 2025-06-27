import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore
from utils import get_threads


class ConcKyotoCabinetExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, bthreads = get_threads()

        for lock in locks:
            for t in threads:
                self.tests.append(
                    {
                        "name": f"KyotoCabinet with {lock} lock and concurrent workload with {t} threads",
                        "benchmark": {
                            "id": "kyotocabinet",
                            "args": {
                                "lock": lock,
                                "threads": bthreads if bthreads else t,
                                "num": 50000,
                                "benchmarks": [
                                    "fillrandom",
                                    "readrandom",
                                ],
                            },
                        },
                        "concurrent": {
                            "id": "scheduling",
                            "args": {
                                "lock": "mcstas",
                                "base-threads": t,
                                "num-threads": t,
                                "step-duration": 10000000,  # Will be killed when LevelDB finishes
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
                x="concurrent_num-threads",
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
                x="concurrent_num-threads",
                y=col,
                hue="lock",
                style="lock",
                markers=True,
            )
            ax2.set_ylim(ymin, ymax)

            plt.xlabel(
                f"Concurrent Threads (With {results['threads'].get(0)} LevelDB threads)"
            )
            plt.ylabel("Latency (micros/op)")
            plt.title(
                f"KyotoCabinet {bench_name} Concurrent Workload Benchmark (Lower is better)"
            )
            plt.legend(title="Lock")
            plt.grid(True)
            plt.ylim(bottom=0)

            output_path = os.path.join(exp_dir, f"{bench_name}.png")
            plt.savefig(output_path, dpi=600, bbox_inches="tight")
            print(f"Wrote plot to {output_path}")
