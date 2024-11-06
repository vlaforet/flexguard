import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class LevelDBExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "BPF Hybrid Lock": ("hybridv2", [100000, 10000, 1000]),
            "BPF Hybrid Lock No Next Waiter Sleeping Detection": (
                "hybridv2nonextwaiterdetection",
                [100000, 10000, 1000],
            ),
            "Pthread Mutex": ("mutex", [100000, 10000, 1000]),
            # "Stock": ("stock", [100000, 10000, 1000]),
            "MCS": ("mcs", [100000, 1000, 10, 10, 10]),
        }

        threads = [1, 2] + [i for i in range(10, 182, 10)]

        for label, (lock, steps) in locks.items():
            num_step = max((len(threads) + 1) // len(steps), 1)
            for k, t in enumerate(threads):

                self.tests.append(
                    {
                        "benchmark": "leveldb",
                        "name": f"LevelDB with {label} lock and {t} threads",
                        "label": label,
                        "kwargs": {
                            "lock": lock,
                            "threads": t,
                            "num": steps[min(k // num_step, len(steps) - 1)],
                        },
                    }
                )

    def report(self, results, exp_dir):
        results_ylim = results[
            ~results["lock"].isin(["mcs", "hybridv2nonextwaiterdetection"])
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
