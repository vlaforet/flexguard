from experiments.experimentCore import ExperimentCore
from utils import get_threads


class ConcDedupExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, bthreads = get_threads()

        for lock in locks:
            for t in threads:
                for comp in ["gzip"]:
                    for compress in [True]:
                        self.tests.append(
                            {
                                "name": f"Dedup with {lock} lock and {comp} compression (compress={compress}) and concurrent workload with {t} threads",
                                "benchmark": {
                                    "id": "dedup",
                                    "args": {
                                        "lock": lock,
                                        "threads": bthreads if bthreads else t,
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
                                        "step-duration": 300000000,  # Will be killed when Dedup finishes
                                    },
                                },
                            }
                        )

    def report(self, results, exp_dir):
        pass
