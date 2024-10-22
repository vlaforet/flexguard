from experiments.experimentCore import ExperimentCore


class IndexExperiment(ExperimentCore):
    def __init__(self, with_debugging):
        super().__init__(with_debugging)

        indexes = {
            "B+-tree BPF Hybrid Lock": "btreelc_bhl",
            "B+-tree MCS/TAS": "btreelc_mcstas",
            "B+-tree Pthread Mutex": "btreelc_mutex",
            "ART BPF Hybrid Lock": "artlc_bhl",
            "ART MCS/TAS": "artlc_mcstas",
            "ART Pthread Mutex": "artlc_mutex",
        }

        if self.with_debugging:
            indexes["B+-tree BPF Hybrid Lock debug"] = "btreelc_bhl"

        # threads = [1, 2] + [i for i in range(10, 182, 10)]
        threads = [40, 60, 70, 180]
        # threads = [10, 20]

        for label, index in indexes.items():
            for t in threads:
                self.tests.append(
                    {
                        "benchmark": "index",
                        "name": f"Index {label} {t} threads",
                        "label": label,
                        "kwargs": {
                            "index": index,
                            "threads": t,
                            "mode": "time",
                            "read_ratio": 0,
                            "update_ratio": 1,
                            "seconds": 10,
                            "records": 100_000_000,
                            "distribution": "SELFSIMILAR",
                            "skew": 0.2,
                        },
                    }
                )
