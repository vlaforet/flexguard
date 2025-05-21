from experiments.experimentCore import ExperimentCore


class HackbenchExperiment(ExperimentCore):
    def __init__(self, locks):
        super().__init__(locks)

        hackbench_kwargs = {
            "fds": 25,
            "groups": 26,
            "loops": 10000,
            "datasize": 512,
        }

        self.tests = [
            {
                "name": "Hackbench FlexGuard",
                "benchmark": {"id": "hackbench", "args": hackbench_kwargs},
                "concurrent": {
                    "id": "init",
                    "args": {
                        "lock": "flexguard",
                    },
                },
            },
            {
                "name": "Hackbench",
                "benchmark": {"id": "hackbench", "args": hackbench_kwargs},
                "concurrent": {
                    "id": "init",
                    "args": {
                        "lock": "mutex",
                    },
                },
            },
            {
                "name": "Hackbench",
                "benchmark": {"id": "hackbench", "args": hackbench_kwargs},
            },
        ]

    def report(self, results, exp_dir):
        average_times = results.groupby("lock")["time"].mean()

        locks = average_times.index.tolist()
        for case_a in locks:
            for case_b in locks:
                if case_a == case_b:
                    continue

                val_a = average_times[case_a]
                val_b = average_times[case_b]

                rel_diff = (val_b - val_a) / val_a * 100
                print(f"{case_b} is {rel_diff:.2f}% slower than {case_a}")
