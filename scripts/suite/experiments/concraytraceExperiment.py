import os

import matplotlib.pyplot as plt
import seaborn as sns
from experiments.experimentCore import ExperimentCore


class ConcDedupExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)
        bthreads = 52

        threads = [1, 2, 4, 8, 16, 32, 48, 50, 52, 54, 64, 128, 256]

        for lock in locks:
            for t in threads:
                if t + bthreads > 104 and lock in ["mcs", "mcstp", "malthusian"]:
                    continue
                self.tests.append(
                    {
                        "name": f"Dedup with {lock} lock and concurrent workload with {t} threads",
                        "benchmark": {
                            "id": "raytrace",
                            "args": {
                                "lock": lock,
                                "threads": bthreads,
                            },
                        },
                        "concurrent": {
                            "id": "scheduling",
                            "args": {
                                "lock": "mcstas",
                                "base-threads": t,
                                "num-threads": t,
                                "step-duration": 300000000,  # Will be killed when Dedup finishes
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        pass
