from experiments.experimentCore import ExperimentCore
from utils import get_threads


class ConcIndexExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, bthreads = get_threads()

        for lock in locks:
            for t in threads:
                self.tests.append(
                    {
                        "name": f"Index with {lock} lock and concurrent workload with {t} threads",
                        "benchmark": {
                            "id": "index",
                            "args": {
                                "index": f"btreelc_{lock}",
                                "threads": bthreads if bthreads else t,
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
