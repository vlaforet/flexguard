from experiments.experimentCore import ExperimentCore


class CorrectnessExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        locks = {
            "BPF Hybrid Lock": "hybridv2",
            "MCS/TAS": "mcstas",
            "MCS": "mcs",
            "Spin Extend Time Slice": "spinextend",
            "Pthread Mutex": "mutex",
        }

        threads = [1, 2] + [x for x in range(10, 190, 20)]

        for label, lock in locks.items():
            for thread in threads:
                self.tests.append(
                    {
                        "name": f"Correctness of {label} lock with {thread} threads",
                        "label": label,
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
