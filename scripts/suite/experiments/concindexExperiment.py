import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class ConcIndexExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)
        bthreads = 52

        threads = [1, 2, 4, 8, 16, 32, 48, 50, 52, 54, 64, 128, 256]

        for lock in locks:
            for t in threads:
                if t + bthreads >= 104 and lock in ["mcs", "mcstp", "malthusian"]:
                    continue

                self.tests.append(
                    {
                        "name": f"Index with {lock} lock and concurrent workload with {t} threads",
                        "benchmark": {
                            "id": "index",
                            "args": {
                                "index": f"btreelc_{lock}",
                                "threads": bthreads,
                                "mode": "time",
                                "read_ratio": 0,
                                "update_ratio": 1,
                                "seconds": 10,
                                "records": 100_000_000,
                                "distribution": "SELFSIMILAR",
                                "skew": 0.2,
                            },
                        },
                        "concurrent": {
                            "id": "scheduling",
                            "args": {
                                "lock": "mcstas",
                                "base-threads": t,
                                "num-threads": t,
                                "step-duration": 30000,  # Will be killed when Index finishes
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        pass
