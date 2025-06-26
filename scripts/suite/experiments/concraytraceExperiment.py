from experiments.experimentCore import ExperimentCore
from utils import get_threads


class ConcRaytraceExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, bthreads = get_threads()

        for lock in locks:
            for t in threads:
                self.tests.append(
                    {
                        "name": f"Raytrace with {lock} lock and concurrent workload with {t} threads",
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
                                "step-duration": 300000000,  # Will be killed when Raytrace finishes
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        pass
