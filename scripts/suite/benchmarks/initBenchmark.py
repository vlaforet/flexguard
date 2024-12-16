import os
import re
import subprocess

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class InitBenchmark(BenchmarkCore):
    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)

    def estimate_runtime(self, **kwargs):
        if "duration" in kwargs:
            return kwargs["duration"]
        if "d" in kwargs:
            return kwargs["d"]
        return None

    def run(self, **kwargs):
        test_init_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "lock"]

        commands = [
            os.path.join(self.base_dir, f"test_init_{kwargs['lock']}"),
            *test_init_args,
        ]
        print(" ".join(commands))

        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run test_init ({result.returncode}):", result.stderr)
            return None

        return pd.DataFrame([{}])
