from experiments.experimentCore import ExperimentCore


class BucketsExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        locks = ["hybridv2", "mcstas", "mcs", "mutex"]
        threads = [1, 2] + [x for x in range(10, 190, 20)]

        for lock in locks:
            for thread in threads:
                self.tests.append(
                    {
                        "benchmark": "buckets",
                        "name": f"Buckets using {lock} lock and {thread} threads",
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
