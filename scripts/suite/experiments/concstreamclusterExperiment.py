from experiments.experimentCore import ExperimentCore
from utils import get_threads


class ConcStreamclusterExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, bthreads = get_threads()

        for lock in locks:
            for t in threads:
                self.tests.append(
                    {
                        "name": f"Streamcluster with {lock} lock and concurrent workload with {t} threads",
                        "benchmark": {
                            "id": "streamcluster",
                            "args": {
                                "lock": lock,
                                "threads": bthreads,
                                "num_points": 16384,
                                "chunksize": 16384,
                            },
                        },
                        "concurrent": {
                            "id": "scheduling",
                            "args": {
                                "lock": "mcstas",
                                "base-threads": t,
                                "num-threads": t,
                                "step-duration": 300000000,  # Will be killed when Streamcluster finishes
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        pass
