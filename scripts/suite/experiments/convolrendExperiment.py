from experiments.experimentCore import ExperimentCore
from utils import get_threads


class ConcVolrendExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, bthreads = get_threads()

        for lock in locks:
            for t in threads:
                self.tests.append(
                    {
                        "name": f"Volrend with {lock} lock and concurrent workload with {t} threads",
                        "benchmark": {
                            "id": "volrend",
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
                                "step-duration": 300000000,  # Will be killed when Volrend finishes
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        pass
