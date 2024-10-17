import os
import re
import subprocess

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class CorrectnessBenchmark(BenchmarkCore):
    pattern = re.compile(r"Counter total\s*:\s*(\d+),\s*Expected\s*:\s*(\d+)")

    def __init__(self, base_dir):
        super().__init__(base_dir)

    def combine_replications(self, *results):
        return (
            pd.concat(results)
            .groupby(level=0)
            .aggregate({"correct": "all"})
            .reset_index(drop=True)
        )

    def estimate_runtime(self, **kwargs):
        if "duration" in kwargs:
            return kwargs["duration"]
        if "d" in kwargs:
            return kwargs["d"]
        return None

    def run(self, **kwargs):
        test_correctness_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "lock"]

        commands = [
            os.path.join(self.base_dir, f"test_correctness_{kwargs['lock']}"),
            *test_correctness_args,
        ]
        print(" ".join(commands))

        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(
                f"Failed to run test_correctness ({result.returncode}):", result.stderr
            )
            return None

        results = {
            "correct": m.group(1) == m.group(2)
            for line in result.stdout.splitlines()
            if (m := self.pattern.match(line))
        }
        return pd.DataFrame([results]) if results else None
