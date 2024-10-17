from experiments.experimentCore import ExperimentCore


class IndexExperiment(ExperimentCore):
    def __init__(self):
        super().__init__()

        index_bases = [
            "btreelc_bhl",
            "btreelc_mcstas",
            "btreelc_mutex",
            "artlc_bhl",
            "artlc_mcstas",
            "artlc_mutex",
        ]

        # page_size_suffixes = ["", "_512", "_1K", "_2K", "_4K", "_8K", "_16K"]
        page_size_suffixes = [""]

        # threads = [1, 2] + [i for i in range(10, 182, 10)]
        threads = [40, 60, 70, 180]
        # threads = [10, 20]

        for index in [
            index + suffix for index in index_bases for suffix in page_size_suffixes
        ]:
            for t in threads:
                self.tests.append(
                    {
                        "benchmark": "index",
                        "name": f"Index {index} {t} threads",
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
