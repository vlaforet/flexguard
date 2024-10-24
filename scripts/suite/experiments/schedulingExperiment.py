from experiments.experimentCore import ExperimentCore


class SchedulingExperiment(ExperimentCore):
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
                    "benchmark": "scheduling",
                    "name": f"Scheduling using {label} lock",
                    "label": label,
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
