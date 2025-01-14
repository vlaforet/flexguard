from experiments.experimentCore import ExperimentCore


class HackbenchExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        hackbench_kwargs = {
            "fds": 25,
            "groups": 26,
            "loops": 10000,
            "datasize": 512,
        }

        self.tests = [
            {
                "name": "Hackbench Hybridv2",
                "label": "LoadRunner",
                "benchmark": {"id": "hackbench", "args": hackbench_kwargs},
                "concurrent": {
                    "id": "init",
                    "args": {
                        "lock": "hybridv2",
                    },
                },
            },
            {
                "name": f"Hackbench",
                "label": "POSIX",
                "benchmark": {"id": "hackbench", "args": hackbench_kwargs},
                "concurrent": {
                    "id": "init",
                    "args": {
                        "lock": "mutex",
                    },
                },
            },
            {
                "name": f"Hackbench",
                "label": "Default",
                "benchmark": {"id": "hackbench", "args": hackbench_kwargs},
            },
        ]

    def report(self, results, exp_dir):
        average_times = results.groupby("label")["time"].mean()

        labels = average_times.index.tolist()
        for caseA in labels:
            for caseB in labels:
                if caseA == caseB:
                    continue

                valA = average_times[caseA]
                valB = average_times[caseB]

                rel_diff = (valB - valA) / valA * 100
                print(f"{caseB} is {rel_diff:.2f}% slower than {caseA}")
