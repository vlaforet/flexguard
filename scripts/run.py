#!/usr/bin/env python3

import argparse
import hashlib
import json
import os
import signal
import sys
from typing import List

import numpy as np
import pandas as pd
from experiments.experimentCore import ExperimentCore
from plugins import getBenchmark, getExperiments

np.seterr("raise")

base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(base_dir)
results_dir = os.path.join(base_dir, "results")


def estimates(exps: List[ExperimentCore]):
    """Returns estimations for the runtime and number of tests
    for one replication."""
    time_estimate = 0
    for exp in exps:
        for test in exp.tests:
            b = getBenchmark(test["benchmark"], base_dir)
            time_estimate += b.estimate_runtime(**test["kwargs"]) or 0

    return sum(len(exp.tests) for exp in exps), time_estimate


if __name__ == "__main__":
    signal.signal(signal.SIGINT, lambda _, __: sys.exit(0))

    parser = argparse.ArgumentParser(
        description="Run benchmarks and export in a standard way"
    )
    parser.add_argument(
        "-r",
        dest="replication",
        type=int,
        default=3,
        help="Number of replication of each test (default=3)",
    )
    parser.add_argument(
        "--no-cache",
        dest="cache",
        action="store_false",
        default=True,
        help="Ignore cache",
    )
    parser.add_argument(
        "-e",
        dest="experiments",
        type=str,
        nargs="+",
        default=None,
        help="Experiments to run (default=None (run all))",
    )
    parser.add_argument(
        "-o",
        dest="output_file",
        type=str,
        default="out.png",
        help='Output file (default="out.png")',
    )
    args = parser.parse_args()

    exps = getExperiments(args.experiments)
    if exps is None:
        print("No experiment to run has been specified")
        sys.exit(1)

    tests_count, time_estimate = estimates(exps)
    print(f"Estimated run time: {time_estimate*args.replication/1000}s")
    tests_count *= args.replication

    test_id = 0
    for exp in exps:
        print(f"Running experiment {exp.name}")
        exp_dir = os.path.join(results_dir, exp.name)
        os.makedirs(os.path.join(exp_dir, "cache"), exist_ok=True)

        results = {}
        for i in range(args.replication):
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

                if args.cache and os.path.exists(cache_file):
                    res = pd.read_csv(cache_file)
                    print(
                        f"[{test_id}/{tests_count}] Retrieved cached test: {test['name']}"
                    )
                else:
                    b = getBenchmark(test["benchmark"], base_dir)

                    print(f"[{test_id}/{tests_count}] Running test: {test['name']}")
                    res = b.run(**test["kwargs"])
                    if res is None:
                        print(f"Test {test['name']} failed")
                        continue

                    if args.cache:
                        res.to_csv(cache_file, index=False)

                if k not in results:
                    results[k] = []
                results[k].append(res)

        rows = []
        for k, test in enumerate(exp.tests):
            if k not in results:
                continue

            b = getBenchmark(test["benchmark"], base_dir)
            res = b.combine_replications(*results[k])

            row = {
                "test_name": test["name"],
                "replications": len(results[k]),
                **test["kwargs"],
            }
            rows.append(pd.merge(res, pd.DataFrame([row]), "cross"))

        if len(rows) == 0:
            print("No output")
        else:
            df = pd.concat(rows, ignore_index=True)
            df.to_csv(os.path.join(exp_dir, f"{exp.name}.csv"))
