import os
import re
import subprocess

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class SchedulingBenchmark(BenchmarkCore):
    pattern = re.compile(r"(\d+),\s*(\d+),\s*([+-]?\d*\.\d+)")

    def __init__(self, base_dir):
        super().__init__(base_dir)

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

        rows = [
            {
                "id": int(m.group(1)),
                "threads": int(m.group(2)),
                "throughput": float(m.group(3)),
            }
            for line in result.stdout.splitlines()
            if (m := self.pattern.match(line))
        ]
        return pd.DataFrame(rows) if rows else None
