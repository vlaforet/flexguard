import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class ConcLevelDBExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)
        bthreads = 32

        locks = {
            "LoadRunner": "hybridv2",
            "POSIX": "mutex",
            "MCS": "mcs",
        }

        threads = [
            1,
            5,
            10,
            15,
            20,
            25,
            30,
            40,
            50,
            60,
            70,
            80,
            90,
            100,
            110,
            130,
            150,
            170,
            190,
            210,
        ]

        for label, lock in locks.items():
            for t in threads:
                if lock == "mcs" and t + bthreads > 70:
                    continue

                self.tests.append(
                    {
                        "benchmark": "leveldb",
                        "concurrent": {
                            "benchmark": "scheduling",
                            "kwargs": {
                                "lock": "mcstas",
                                "base-threads": t,
                                "num-threads": t,
                                "step-duration": 100000000000,  # Will be killed when LevelDB finishes
                            },
                        },
                        "name": f"LevelDB with {label} lock and concurrent workload with {t} threads",
                        "label": label,
                        "kwargs": {
                            "lock": lock,
                            "threads": bthreads,
                            "num": 100000,
                            "benchmarks": [
                                "fillseq",
                                "fillsync",
                                "fillrandom",
                                "overwrite",
                                "readrandom",
                            ],
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
                hue="label",
                style="label",
                markers=True,
            )
            ymin, ymax = ax.get_ylim()
            ax.remove()

            sns.lineplot()

            ax2 = sns.lineplot(
                data=results,
                x="concurrent_num-threads",
                y=col,
                hue="label",
                style="label",
                markers=True,
            )
            ax2.set_ylim(ymin, ymax)

            plt.xlabel(
                f"Concurrent Threads (With {results['threads'].get(0)} LevelDB threads)"
            )
            plt.ylabel("Latency (micros/op)")
            plt.title(
                f"LevelDB {bench_name} Concurrent Workload Benchmark (Lower is better)"
            )
            plt.legend(title="Lock")
            plt.grid(True)
            plt.ylim(bottom=0)

            output_path = os.path.join(exp_dir, f"{bench_name}.png")
            plt.savefig(output_path, dpi=600, bbox_inches="tight")
            print(f"Wrote plot to {output_path}")
