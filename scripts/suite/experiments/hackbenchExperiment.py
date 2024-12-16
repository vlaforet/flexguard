from experiments.experimentCore import ExperimentCore


class HackbenchExperiment(ExperimentCore):
    tests = []

    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        hackbench_kwargs = {
            "fds": 25,
            "groups": 26,
            "loops": 10000,
            "datasize": 512,
        }

        self.tests = [
            {
                "benchmark": "hackbench",
                "concurrent": {
                    "benchmark": "init",
                    "kwargs": {
                        "lock": "hybridv2",
                    },
                },
                "name": "Hackbench Hybridv2",
                "label": "LoadRunner",
                "kwargs": hackbench_kwargs,
            },
            {
                "benchmark": "hackbench",
                "concurrent": {
                    "benchmark": "init",
                    "kwargs": {
                        "lock": "mutex",
                    },
                },
                "name": f"Hackbench",
                "label": "POSIX",
                "kwargs": hackbench_kwargs,
            },
            {
                "benchmark": "hackbench",
                "name": f"Hackbench",
                "label": "Default",
                "kwargs": hackbench_kwargs,
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
