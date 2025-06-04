import os
import re
import subprocess

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import sha256_hash_file


class CorrectnessBenchmark(BenchmarkCore):
    pattern = re.compile(r"Counter total\s*:\s*(\d+),\s*Expected\s*:\s*(\d+)")

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)

    def estimate_runtime(self, **kwargs):
        if "duration" in kwargs:
            return kwargs["duration"]
        if "d" in kwargs:
            return kwargs["d"]
        return None

    def get_run_hash(self, **kwargs):
        exec_hash = sha256_hash_file(
            os.path.join(self.base_dir, "build", f"test_correctness_{kwargs['lock']}")
        )
        if exec_hash is None:
            raise Exception("Failed to hash executable.")

        return super().get_run_hash(exec_hash=exec_hash, kwargs=kwargs)

    def run(self, **kwargs):
        test_correctness_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "lock"]

        commands = [
            os.path.join(self.base_dir, "build", f"test_correctness_{kwargs['lock']}"),
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
