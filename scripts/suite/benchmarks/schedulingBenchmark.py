import os
import re
import subprocess

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import sha256_hash_file


class SchedulingBenchmark(BenchmarkCore):
    pattern = re.compile(r"(\d+),\s*(\d+),\s*([+-]?\d*\.\d+)")

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)

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

    def get_run_hash(self, **kwargs):
        exec_hash = sha256_hash_file(
            os.path.join(self.base_dir, f"scheduling_{kwargs['lock']}")
        )
        if exec_hash is None:
            raise Exception("Failed to hash executable.")

        return super().get_run_hash(exec_hash=exec_hash, kwargs=kwargs)

    def run(self, **kwargs):
        scheduling_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "lock"]

        commands = [
            os.path.join(self.base_dir, f"scheduling_{kwargs['lock']}"),
            *scheduling_args,
        ]
        print(" ".join(commands))

        try:
            result = subprocess.run(
                commands,
                capture_output=True,
                text=True,
                timeout=1.3 / 1000 * self.estimate_runtime(**kwargs),
            )
            if result.returncode != 0:
                print(f"Failed to run scheduling ({result.returncode}):", result.stderr)
                return None
        except Exception as e:
            print(f"Failed to run scheduling:", e)
            return None

        rows = [
            {
                "id": int(m.group(1)),
                "threads": int(m.group(2)),
                "value": float(m.group(3)),
            }
            for line in result.stdout.splitlines()
            if (m := self.pattern.match(line))
        ]
        return pd.DataFrame(rows) if rows else None
