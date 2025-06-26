from experiments.experimentCore import ExperimentCore
from utils import get_threads


class DedupExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, _ = get_threads()

        for lock in locks:
            for t in threads:
                for comp in ["gzip"]:
                    for compress in [True]:
                        self.tests.append(
                            {
                                "name": f"Dedup with {lock} lock and with {t} threads and {comp} compression (compress={compress})",
                                "benchmark": {
                                    "id": "dedup",
                                    "args": {
                                        "lock": lock,
                                        "threads": t,
                                        "compress": compress,
                                        "compression_type": comp,
                                    },
                                },
                            }
                        )

    def report(self, results, exp_dir):
        pass
