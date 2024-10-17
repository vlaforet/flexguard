import os
import re
import subprocess
import sys
from math import prod

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import pygal
import seaborn as sns
from scripts.benchmarks.benchmarkCore import BenchmarkCore


class SchedulingBenchmark(BenchmarkCore):
    def __init__(self, base_dir):
        super().__init__(base_dir)

    def combine_replications(self, *results):
        return (
            pd.concat(results)
            .groupby(["id"])
            .agg({"throughput": "mean", "threads": "mean"})
            .reset_index()
        )

    def estimate_runtime(self, **kwargs):
        steps = 0

        if "num-threads" in kwargs:
            steps = kwargs["num-threads"]
        else:
            steps = 10

        if "base-threads" in kwargs:
            steps -= kwargs["base-threads"]

        if "thread-step" in kwargs:
            steps /= kwargs["thread-step"]

        if "increasing-only" not in kwargs or kwargs["increasing-only"] != 1:
            steps = steps * 2 + 5

        return (
            steps * kwargs["step-duration"]
            if "step-duration" in kwargs
            else steps * 1000
        )

    def run(self, **kwargs):
        scheduling_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "lock"]

        commands = [
            os.path.join(self.base_dir, f"scheduling_{kwargs['lock']}"),
            *scheduling_args,
        ]
        print(" ".join(commands))

        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run scheduling ({result.returncode}):", result.stderr)
            return None

        pattern = r"(\d+),\s*(\d+),\s*([+-]?\d*\.\d+)"
        rows = []
        for line in result.stdout.split("\n"):
            m = re.match(pattern, line)
            if m:
                id, threads, throughput = m.groups()
                rows.append(
                    {
                        "id": int(id),
                        "threads": int(threads),
                        "throughput": float(throughput),
                    }
                )

        if len(rows) == 0:
            return None
        return pd.DataFrame(rows)
