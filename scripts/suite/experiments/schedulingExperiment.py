import os

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class SchedulingExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "MCS": "mcs",
            # "MCS-TAS": "mcstas",
            # "MCS-Block": "mcsblock",
            "POSIX": "mutex",
            "Pure blocking lock": "futex",
            "MCS-TP": "mcstp",
            # "Spin-Then-Park": "spinpark",
            "Shfllock": "shuffle",
            "Malthusian": "malthusian",
            "LoadRunner": "hybridv2",
            # "CBO-MCS": "cbomcs",
        }

        for label, lock in locks.items():
            self.tests.append(
                {
                    "benchmark": "scheduling",
                    "name": f"Scheduling using {label} lock",
                    "label": label,
                    "kwargs": {
                        "lock": lock,
                        "latency": 1,
                        "base-threads": 1,
                        "num-threads": 250,
                        "step-duration": 2500,
                        "cache-lines": 5,
                        "thread-step": 1,
                        "increasing-only": 0,
                    },
                }
            )

    def report_latency(self, results: pd.DataFrame, exp_dir):
        results_ylim = results[~results["lock"].isin(["mcs"])]

        plt.figure(figsize=(10, 6))
        ax = sns.lineplot(
            data=results_ylim,
            x="id",
            y="value",
            hue="label",
            style="label",
            markers=True,
        )
        _, ymax = ax.get_ylim()
        ax.remove()

        ax2 = sns.lineplot(
            data=results,
            x="id",
            y="value",
            hue="label",
            style="label",
            markers=True,
        )
        ax2.set_ylim(0, ymax)

        id_threads = results.groupby("id")["threads"].first()
        ax2.set_xticklabels([id_threads.get(t, None) for t in ax2.get_xticks()])

        plt.title("Single-lock Latency Microbenchmark (Lower is better)")
        plt.xlabel("Threads")
        plt.ylabel("Critical Section Latency (micros/cs)")
        plt.grid(True)

        output_path = os.path.join(exp_dir, "latency.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")

    def report_throughput(self, results: pd.DataFrame, exp_dir):
        plt.figure(figsize=(10, 6))
        ax = sns.lineplot(
            data=results,
            x="id",
            y="value",
            hue="label",
            style="label",
            markers=True,
        )

        id_threads = results.groupby("id")["threads"].first()
        ax.set_xticklabels([id_threads.get(t, None) for t in ax.get_xticks()])

        plt.title("Single-lock Throughput Microbenchmark (Higher is better)")
        plt.xlabel("Threads")
        plt.ylabel("Throughput (OPs/s)")
        plt.grid(True)

        output_path = os.path.join(exp_dir, "throughput.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")

    def report(self, results, exp_dir):
        if "latency" not in results.columns:
            return self.report_throughput(results, exp_dir)

        latency_data = results[results["latency"] != 0 & ~results["latency"].isna()]
        if not latency_data.empty:
            return self.report_latency(latency_data, exp_dir)

        throughput_data = results[results["latency"] == 0 | results["latency"].isna()]
        if not throughput_data.empty:
            return self.report_throughput(throughput_data, exp_dir)
