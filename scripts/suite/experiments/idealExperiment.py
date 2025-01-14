import os

import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class IdealExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        locks = {
            "MCS": "mcs",
            "MCS-TAS": "mcstas",
            "MCS-Block": "mcsblock",
            "POSIX": "mutex",
            "Pure blocking lock": "futex",
            "MCS-TP": "mcstp",
            "Spin-Then-Park": "spinpark",
            "Shfllock": "shuffle",
            "Malthusian": "malthusian",
            "FlexGuard": "flexguard",
            # "CBO-MCS": "cbomcs",
        }

        for label, lock in locks.items():
            self.tests.append(
                {
                    "name": f"Ideal using {label} lock",
                    "label": label,
                    "benchmark": {
                        "id": "scheduling",
                        "args": {
                            "lock": lock,
                            "latency": 1,
                            "base-threads": 1,
                            "num-threads": 120,
                            "step-duration": 2500,
                            "cache-lines": 5,
                            "thread-step": 1,
                            "increasing-only": 1,
                        },
                    },
                }
            )

    def report(self, results, exp_dir):
        zero_rows = (results["threads"] == 0) & (results["value"] == 0)
        max_id_per_lock = results.groupby("lock")["id"].transform(max)
        max_id_rows = results["id"] == max_id_per_lock

        clean_results = results[~(zero_rows & max_id_rows)]

        agg_results = clean_results.groupby(["lock", "id"], as_index=False).agg(
            {
                "value": "mean",
                **{c: "first" for c in clean_results.columns if c != "value"},
            }
        )

        filtered_results = agg_results[agg_results["lock"].isin(["mcs", "futex"])]
        ideal_results = filtered_results.loc[
            filtered_results.groupby("id")["value"].idxmin().reset_index(drop=True)
        ]
        ideal_results["lock"] = "ideal"
        ideal_results["label"] = "Ideal Hybrid Lock"

        full_results_tmp = pd.concat([agg_results, ideal_results], ignore_index=True)
        full_results = full_results_tmp[full_results_tmp["id"] < 120]
        # full_results = full_results_tmp

        mutex_values = full_results.loc[full_results["lock"] == "futex"].set_index(
            "id"
        )["value"]
        full_results["normalized_value"] = full_results["value"] / full_results[
            "id"
        ].map(mutex_values)
        full_results.to_csv("/tmp/out.csv")

        plt.figure(figsize=(10, 6))
        ax = sns.lineplot(
            data=full_results,
            x="id",
            y="normalized_value",
            hue="label",
            style="label",
            markers=False,
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
        # plt.xlim(0, 11)
        plt.yscale("log")
        plt.ylim(0.5, 1.5)
        # plt.ylim(0, ymax)

        output_path = os.path.join(exp_dir, "ideal.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote full plot to {output_path}")
