from experiments.experimentCore import ExperimentCore
from utils import get_threads


class StreamclusterExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, _ = get_threads()

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
