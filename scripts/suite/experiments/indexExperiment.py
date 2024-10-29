import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class IndexExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        indexes = {
            "B+-tree BPF Hybrid Lock": "btreelc_bhl",
            "B+-tree MCS/TAS": "btreelc_mcstas",
            "B+-tree Pthread Mutex": "btreelc_mutex",
            "ART BPF Hybrid Lock": "artlc_bhl",
            "ART MCS/TAS": "artlc_mcstas",
            "ART Pthread Mutex": "artlc_mutex",
        }

        if self.with_debugging:
            indexes["B+-tree BPF Hybrid Lock debug"] = "btreelc_bhl"

        threads = [1, 2] + [i for i in range(10, 182, 10)]
        # threads = [40, 60, 70, 180]
        # threads = [10, 20]

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
        artlc_data = results[results["index"].str.startswith("artlc")]
        plt.figure(figsize=(10, 6))
        sns.lineplot(
            data=artlc_data,
            x="threads",
            y="succeeded",
            hue="label",
            style="label",
            markers=True,
        )

        plt.xlabel("Threads")
        plt.ylabel("Operations succeeded")
        plt.title("Succeeded Operations by Threads (artlc Indexes)")
        plt.legend(title="Index")
        plt.grid(True)
        plt.ylim(bottom=0)

        output_path = os.path.join(exp_dir, "artlc.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")

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
        plt.ylabel("Operations succeeded")
        plt.title("Succeeded Operations by Threads (btreelc Indexes)")
        plt.legend(title="Index")
        plt.grid(True)
        plt.ylim(bottom=0)

        output_path = os.path.join(exp_dir, "btreelc.png")
        plt.savefig(output_path, dpi=600, bbox_inches="tight")
        print(f"Wrote plot to {output_path}")
