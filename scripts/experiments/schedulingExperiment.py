from experiments.experimentCore import ExperimentCore


class SchedulingExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        locks = ["hybridv2", "mcstas", "mcs", "mutex"]

        for lock in locks:
            self.tests.append(
                {
                    "benchmark": "scheduling",
                    "name": f"Scheduling using {lock} lock",
                    "kwargs": {
                        "lock": lock,
                        "base-threads": 0,
                        "num-threads": 180,
                        "step-duration": 5000,
                        "cache-lines": 5,
                        "thread-step": 1,
                        "increasing-only": 1,
                    },
                }
            )
