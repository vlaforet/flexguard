import os
import re
import subprocess
import sys

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class IndexBenchmark(BenchmarkCore):
    opTypes = ["Insert", "Read", "Update", "Remove", "Scan"]
    finishTypes = ["completed", "succeeded"]

    def __init__(self, base_dir):
        super().__init__(base_dir)
        self.index_dir = os.path.join(self.base_dir, "ext", "index-benchmarks")

        self.patterns = {
            f"{opType}-{finishType}": re.compile(
                rf"\s+\-\s{opType}\s{finishType}\:\s(.+)\sops"
            )
            for finishType in self.finishTypes
            for opType in self.opTypes
        }

        self.pibench_bin = os.path.join(
            self.index_dir, "build/_deps/pibench-build/src/PiBench"
        )
        if not os.path.isfile(self.pibench_bin):
            print("PiBench binary not found")
            sys.exit(1)
        if not os.access(self.pibench_bin, os.R_OK | os.W_OK | os.X_OK):
            print("PiBench binary permission denied")
            sys.exit(1)

    def combine_replications(self, *results):
        return pd.concat(results).groupby(level=0).mean()

    def estimate_runtime(self, **kwargs):
        if "mode" in kwargs and kwargs["mode"] == "time":
            return kwargs["seconds"] * 1000 if "seconds" in kwargs else 20000
        return 3000

    def run(self, **kwargs):
        pibench_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "index"]

        commands = [
            self.pibench_bin,
            os.path.join(
                self.index_dir, f"build/wrappers/lib{kwargs['index']}_wrapper.so"
            ),
            *pibench_args,
            "--bulk_load",
            "--pcm=False",
            "--skip_verify=True",
            "--apply_hash=False",
        ]
        print(" ".join(commands))

        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run PiBench ({result.returncode}):", result.stderr)
            return None

        results = {}
        for line in result.stdout.splitlines():
            for col, pattern in self.patterns.items():
                if m := pattern.match(line):
                    results[col] = float(m.group(1))
                    break

        for finishType in self.finishTypes:
            results[finishType] = sum(
                results.get(f"{opType}-{finishType}", 0) for opType in self.opTypes
            )

        return pd.DataFrame([results]) if results else None
