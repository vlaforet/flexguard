from experiments.experimentCore import ExperimentCore


class BucketsExperiment(ExperimentCore):
    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        locks = {
            "BPF Hybrid Lock": "hybridv2",
            "MCS/TAS": "mcstas",
            "MCS": "mcs",
            "Pthread Mutex": "mutex",
        }

        threads = [1, 2] + [x for x in range(10, 190, 20)]

        for label, lock in locks.items():
            for thread in threads:
                self.tests.append(
                    {
                        "benchmark": "buckets",
                        "name": f"Buckets using {label} lock and {thread} threads",
                        "label": label,
                        "kwargs": {
                            "lock": lock,
                            "duration": 10000,
                            "num-threads": thread,
                            "buckets": 100,
                            "max-value": 100000,
                            "offset-changes": 40,
                        },
                    }
                )
