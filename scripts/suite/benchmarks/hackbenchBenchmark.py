import re
import subprocess
import time

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class HackbenchBenchmark(BenchmarkCore):
    pattern = re.compile(r"Time:\s+(\d+\.\d+)")

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)

    def estimate_runtime(self, **kwargs):
        return None

    def run(self, **kwargs):
        test_hackbench_args = [f"--{k}={w}" for k, w in kwargs.items()]

        commands = [
            "hackbench",
            *test_hackbench_args,
        ]
        print(" ".join(commands))

        time.sleep(1)  # Allow concurrent workload to start
        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run hackbench ({result.returncode}):", result.stderr)
            return None

        results = {
            "time": m.group(1)
            for line in result.stdout.splitlines()
            if (m := self.pattern.match(line))
        }
        return pd.DataFrame([results]) if results else None
