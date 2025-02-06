import os
from multiprocessing import Process
from typing import Any, List, TypedDict

import pandas as pd
import psutil
from benchmarks.benchmarkCore import BenchmarkCore
from experiments.experimentCore import ExperimentCore
from plugins import getBenchmark
from utils import hash_dict_sha256


class BenchmarkInstance(TypedDict):
    id: str
    args: dict[str, Any]
    instance: BenchmarkCore


class RecordCommand:
    def __init__(
        self,
        base_dir: str,
        temp_dir: str,
        results_dir: str,
        experiments: List[ExperimentCore],
        replication: int,
        cache: bool,
        only_cache: bool,
    ):
        self.base_dir = base_dir
        self.temp_dir = temp_dir
        self.results_dir = results_dir
        self.experiments = experiments
        self.replication = replication
        self.cache = cache
        self.only_cache = only_cache

        self.cache_dir = os.path.join(self.results_dir, "cache")
        os.makedirs(self.cache_dir, exist_ok=True)

    def estimations(self):
        """Returns estimations for the runtime and number of tests
        for one replication."""
        time_estimate = 0
        for exp in self.experiments:
            for test in exp.tests:
                b = getBenchmark(test["benchmark"]["id"], self.base_dir, self.temp_dir)
                time_estimate += b.estimate_runtime(**test["benchmark"]["args"]) or 0

        return sum(len(exp.tests) for exp in self.experiments), time_estimate

    def get_cache_file(
        self,
        benchmark: BenchmarkInstance,
        concurrent: BenchmarkInstance | None,
        replication: int,
    ):
        hash = hash_dict_sha256(
            {
                "benchmark": benchmark["instance"].get_run_hash(**benchmark["args"]),
                "concurrent": (
                    concurrent["instance"].get_run_hash(**concurrent["args"])
                    if concurrent
                    else None
                ),
            }
        )

        cache_file = os.path.join(self.cache_dir, f"{hash}_r{replication}.csv")
        return cache_file

    def get_benchmark_instance(self, bench_config) -> BenchmarkInstance:
        return {
            "id": bench_config["id"],
            "instance": getBenchmark(bench_config["id"], self.base_dir, self.temp_dir),
            "args": bench_config["args"],
        }

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
            for replication in range(self.replication):
                for k, test in enumerate(exp.tests):
                    test_id += 1
                    print(
                        f"[{test_id}/{tests_count}] {test['name']} #{replication}: ",
                        end="",
                    )

                    benchmark = self.get_benchmark_instance(test["benchmark"])
                    concurrent = (
                        self.get_benchmark_instance(test["concurrent"])
                        if "concurrent" in test
                        else None
                    )
                    cache_file = self.get_cache_file(benchmark, concurrent, replication)

                    if self.cache and os.path.exists(cache_file):
                        res = pd.read_csv(cache_file)
                        print("Retrieved")
                    else:
                        if self.only_cache:
                            print("Missing")
                            continue
                        print("Running")
                        try:
                            benchmark["instance"].init(**benchmark["args"])

                            if concurrent:
                                cproc = Process(
                                    target=lambda: (
                                        concurrent["instance"].run(**concurrent["args"])
                                        if concurrent
                                        else None
                                    )
                                )
                                cproc.start()

                            res = benchmark["instance"].run(**benchmark["args"])

                            if concurrent:
                                try:
                                    psproc = psutil.Process(cproc.pid)
                                    psproc.kill()
                                    cproc.join()
                                    for child in psproc.children(recursive=True):
                                        child.kill()
                                except:
                                    pass

                            if res is None:
                                raise Exception()

                            res["replication_id"] = replication
                            res["benchmark"] = benchmark["id"]
                            for arg, val in benchmark["args"].items():
                                res[arg] = val
                            if concurrent:
                                for arg, val in concurrent["args"].items():
                                    res[f"concurrent_{arg}"] = val

                            res.to_csv(cache_file, index=False)
                        except Exception as e:
                            print(f"Test {test['name']} #{replication} failed: {e}")
                            continue

                    if k not in results:
                        results[k] = []
                    results[k].append(res)

            rows = []
            for k, test in enumerate(exp.tests):
                if k not in results:
                    continue

                row = {
                    "test_name": test["name"],
                    "label": test["label"],
                    "replications": len(results[k]),
                }

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
