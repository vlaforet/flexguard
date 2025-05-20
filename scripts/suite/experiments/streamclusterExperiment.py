from experiments.experimentCore import ExperimentCore


class StreamclusterExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads = [
            1,
            2,
            4,
            8,
            16,
            32,
            48,
            64,
            72,
            102,
            104,
            106,
            128,
            192,
            256,
            448,
            512,
        ]

        for lock in locks:
            for t in threads:
                self.tests.append(
                    {
                        "name": f"Streamcluster with {lock} lock and with {t} threads",
                        "benchmark": {
                            "id": "streamcluster",
                            "args": {
                                "lock": lock,
                                "threads": t,
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        pass
