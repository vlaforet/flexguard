from experiments.experimentCore import ExperimentCore
from utils import get_threads


class CorrectnessExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        threads, _ = get_threads()

        for lock in locks:
            for thread in threads:
                self.tests.append(
                    {
                        "name": f"Correctness of {lock} lock with {thread} threads",
                        "benchmark": {
                            "id": "correctness",
                            "args": {
                                "lock": lock,
                                "num-threads": thread,
                                "duration": 10000,
                            },
                        },
                    }
                )

    def report(self, results, exp_dir):
        failed = results[results["correct"] == False]
        if failed.empty:
            print("All locks correct")
        else:
            print("Failed correctness tests:")
            for _, test in failed.iterrows():
                print(f" - {test['test_name']}")
