import os

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class IdealExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "MCS": "mcs",
            "MCS-Block": "mcsblock",
            "Pthread Mutex": "mutex",
            "Pure Blocking Lock": "futex",
            "MCS-TP": "mcstp",
        }

        for label, lock in locks.items():
            self.tests.append(
                {
                    "benchmark": "scheduling",
                    "name": f"Ideal using {label} lock",
                    "label": label,
                    "kwargs": {
                        "lock": lock,
                        "latency": 1,
                        "base-threads": 0,
                        "num-threads": 120,
                        "step-duration": 5000,
                        "cache-lines": 5,
                        "thread-step": 1,
                        "increasing-only": 0,
                    },
                }
            )

    def report(self, results, exp_dir):
        zero_rows = (results["threads"] == 0) & (results["value"] == 0)
        max_id_per_lock = results.groupby("lock")["id"].transform(max)
        max_id_rows = results["id"] == max_id_per_lock

        clean_results = results[~(zero_rows & max_id_rows)]

        agg_results = clean_results.groupby(["lock", "id"], as_index=False).agg(
            {"value": "mean", **{c: "first" for c in clean_results.columns}}
        )

        filtered_results = agg_results[agg_results["lock"].isin(["mcs", "futex"])]
        ideal_results = filtered_results.loc[
            filtered_results.groupby("id")["value"].idxmin().reset_index(drop=True)
        ]
        ideal_results["lock"] = "ideal"
        ideal_results["label"] = "Ideal Hybrid Lock"

        full_results_tmp = pd.concat([agg_results, ideal_results], ignore_index=True)
        full_results = full_results_tmp[full_results_tmp["id"] < 12]

        mutex_values = full_results.loc[full_results["lock"] == "futex"].set_index(
            "id"
        )["value"]
        full_results["normalized_value"] = full_results["value"] / full_results[
            "id"
        ].map(mutex_values)

        results_ylim = full_results[
            ~full_results["lock"].isin(["mcs", "mcsblock", "mcstp"])
        ]
        ax2 = sns.lineplot(
            data=results_ylim,
            x="id",
            y="normalized_value",
            hue="label",
            style="label",
            markers=True,
        )
        ymin, ymax = ax2.get_ylim()

        plt.figure(figsize=(10, 6))
        ax = sns.lineplot(
            data=full_results,
            x="id",
            y="normalized_value",
            hue="label",
            style="label",
            markers=True,
        )

        id_threads = full_results.groupby("id")["threads"].first()
        x_ticks = ax.get_xticks()
        ax.set_xticks(x_ticks)
        ax.set_xticklabels([id_threads.get(t, None) for t in x_ticks])

        plt.title("Single-lock Latency Microbenchmark (Lower is better)")
        plt.xlabel("Threads")
        plt.ylabel(
            "Normalized Critical Section Latency (compared to Pure Blocking Lock)"
        )
        plt.grid(True)
        plt.xlim(0, 11)
        plt.yscale("log")
        # plt.ylim(ymin, ymax * 1.1)
        # plt.ylim(0, ymax)

        output_path = os.path.join(exp_dir, "ideal.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote full plot to {output_path}")
