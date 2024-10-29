from experiments.experimentCore import ExperimentCore


class CorrectnessExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "BPF Hybrid Lock": "hybridv2",
            "MCS/TAS": "mcstas",
            "MCS": "mcs",
            "Pthread Mutex": "mutex",
        }

        for label, lock in locks.items():
            self.tests.append(
                {
                    "benchmark": "correctness",
                    "name": f"Correctness of {label} lock",
                    "label": label,
                    "kwargs": {
                        "lock": lock,
                        "num-threads": 50,
                        "duration": 10000,
                    },
                }
            )
