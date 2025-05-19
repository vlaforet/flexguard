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

                for comp in ["gzip"]:
                    for compress in [True]:
                        self.tests.append(
                            {
                                "name": f"Dedup with {lock} lock and {comp} compression (compress={compress}) and concurrent workload with {t} threads",
                                "benchmark": {
                                    "id": "dedup",
                                    "args": {
                                        "lock": lock,
                                        "threads": bthreads,
                                        "compress": compress,
                                        "compression_type": comp,
                                    },
                                },
                                "concurrent": {
                                    "id": "scheduling",
                                    "args": {
                                        "lock": "mcstas",
                                        "base-threads": t,
                                        "num-threads": t,
                                        "step-duration": 30000,  # Will be killed when Dedup finishes
                                    },
                                },
                            }
                        )

    def report(self, results, exp_dir):
        pass
