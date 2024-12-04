import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class IndexExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        indexes = {
            "LoadRunner": "btreelc_bhl",
            "MCS": "btreelc_mcsrw",
            "POSIX": "btreelc_mutex",
            "MCS-TAS": "btreelc_mcstas",
            "Pure blocking lock": "btreelc_futex",
            "MCS-TP": "btreelc_mcstp",
            "Shfllock": "btreelc_shuffle",
            "Malthusian": "btreelc_malthusian",
        }

        threads = [1] + [i for i in range(5, 300, 5)]

        for label, index in indexes.items():
            for t in threads:
                self.tests.append(
                    {
                        "benchmark": "index",
                        "name": f"Index {label} {t} threads",
                        "label": label,
                        "kwargs": {
                            "index": index,
                            "threads": t,
                            "mode": "time",
                            "read_ratio": 0,
                            "update_ratio": 1,
                            "seconds": 10,
                            "records": 100_000_000,
                            "distribution": "SELFSIMILAR",
                            "skew": 0.2,
                        },
                    }
                )

    def report(self, results, exp_dir):
        if "seconds" in results.columns:
            results["succeeded"] = results["succeeded"] / results["seconds"]

        # artlc_data = results[results["index"].str.startswith("artlc")]
        # plt.figure(figsize=(10, 6))
        # sns.lineplot(
        #    data=artlc_data,
        #    x="threads",
        #    y="succeeded",
        #    hue="label",
        #    style="label",
        #    markers=True,
        # )

        # plt.xlabel("Threads")
        # plt.ylabel("Ops/s")
        # plt.title("ARTLC Indexes Benchmark (Higher is better)")
        # plt.legend(title="Index")
        # plt.grid(True)
        # plt.ylim(bottom=0)

        # output_path = os.path.join(exp_dir, "artlc.png")
        # plt.savefig(output_path, dpi=600, bbox_inches="tight")
        # print(f"Wrote plot to {output_path}")

        btreelc_data = results[results["index"].str.startswith("btreelc")]
        plt.figure(figsize=(10, 6))
        sns.lineplot(
            data=btreelc_data,
            x="threads",
            y="succeeded",
            hue="label",
            style="label",
            markers=True,
        )

        plt.xlabel("Threads")
        plt.ylabel("Throughput (OPs/s)")
        plt.title("B+-tree Indexes Benchmark (Higher is better)")
        plt.legend(title="Index")
        plt.grid(True)
        plt.ylim(bottom=0)

        output_path = os.path.join(exp_dir, "btreelc.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
