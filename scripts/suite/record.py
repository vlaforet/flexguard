import hashlib
import json
import os
from multiprocessing import Process
from typing import List

import pandas as pd
import psutil
from experiments.experimentCore import ExperimentCore
from plugins import getBenchmark


class RecordCommand:
    def __init__(
        self,
        base_dir: str,
        temp_dir: str,
        results_dir: str,
        experiments: List[ExperimentCore],
        replication: int,
        cache: bool,
    ):
        self.base_dir = base_dir
        self.temp_dir = temp_dir
        self.results_dir = results_dir
        self.experiments = experiments
        self.replication = replication
        self.cache = cache

    def estimations(self):
        """Returns estimations for the runtime and number of tests
        for one replication."""
        time_estimate = 0
        for exp in self.experiments:
            for test in exp.tests:
                b = getBenchmark(test["benchmark"], self.base_dir, self.temp_dir)
                time_estimate += b.estimate_runtime(**test["kwargs"]) or 0

        return sum(len(exp.tests) for exp in self.experiments), time_estimate

    def run(self):
        tests_count, time_estimate = self.estimations()
        print(f"Estimated run time: {time_estimate*self.replication/1000}s")
        tests_count *= self.replication

        test_id = 0
        for exp in self.experiments:
            print(f"Running experiment {exp.name}")
            exp_dir = os.path.join(self.results_dir, exp.name)
            os.makedirs(os.path.join(exp_dir, "cache"), exist_ok=True)

            results = {}
            for i in range(self.replication):
                for k, test in enumerate(exp.tests):
                    test_id += 1
                    hash = hashlib.sha256(
                        json.dumps(test, ensure_ascii=True, sort_keys=True).encode()
                    ).hexdigest()

                    cache_file = os.path.join(
                        exp_dir,
                        "cache",
                        f"{hash}_r{i}.csv",
                    )

                    if self.cache and os.path.exists(cache_file):
                        res = pd.read_csv(cache_file)
                        print(
                            f"[{test_id}/{tests_count}] Retrieved cached test: {test['name']} #{i}"
                        )
                    else:
                        b = getBenchmark(
                            test["benchmark"], self.base_dir, self.temp_dir
                        )

                        print(
                            f"[{test_id}/{tests_count}] Running test: {test['name']} #{i}"
                        )

                        if "concurrent" in test:
                            c = getBenchmark(
                                test["concurrent"]["benchmark"],
                                self.base_dir,
                                self.temp_dir,
                            )

                            cproc = Process(
                                target=lambda: c.run(**test["concurrent"]["kwargs"])
                            )
                            cproc.start()

                        res = b.run(**test["kwargs"])

                        if "concurrent" in test:
                            psproc = psutil.Process(cproc.pid)
                            psproc.kill()
                            for child in psproc.children(recursive=True):
                                child.kill()
                            cproc.join()

                        if res is None:
                            print(f"Test {test['name']} failed")
                            continue

                        res["replication_id"] = i
                        if self.cache:
                            res.to_csv(cache_file, index=False)

                    if k not in results:
                        results[k] = []
                    results[k].append(res)

            rows = []
            for k, test in enumerate(exp.tests):
                if k not in results:
                    continue

                b = getBenchmark(test["benchmark"], self.base_dir, self.temp_dir)

                row = {
                    "test_name": test["name"],
                    "label": test["label"],
                    "replications": len(results[k]),
                    **test["kwargs"],
                }

                if "concurrent" in test:
                    row["concurrent_benchmark"] = test["concurrent"]["benchmark"]
                    for key, val in test["concurrent"]["kwargs"].items():
                        row[f"concurrent_{key}"] = val

                rows.append(
                    pd.merge(pd.concat(results[k]), pd.DataFrame([row]), "cross")
                )

            if len(rows) == 0:
                print("No output")
            else:
                df = pd.concat(rows, ignore_index=True)
                output_file = os.path.join(exp_dir, f"{exp.name}.csv")
                df.to_csv(output_file)
                print(f"Recorded {exp.name} experiment results to {output_file}")
