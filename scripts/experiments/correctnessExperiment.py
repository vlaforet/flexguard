from experiments.experimentCore import ExperimentCore


class CorrectnessExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        locks = ["hybridv2", "mcstas", "mcs", "mutex"]

        for lock in locks:
            self.tests.append(
                {
                    "benchmark": "correctness",
                    "name": f"Correctness of {lock} lock",
                    "kwargs": {
                        "lock": lock,
                        "num-threads": 50,
                        "duration": 10000,
                    },
                }
            )
