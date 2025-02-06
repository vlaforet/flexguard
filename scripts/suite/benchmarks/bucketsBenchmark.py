import os
import re
import subprocess

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import sha256_hash_file


class BucketsBenchmark(BenchmarkCore):
    pattern_global = re.compile(r"#Throughput:\s*([\d\.]+)\s*CS/s")
    pattern_local = re.compile(r"#Local result for Thread\s+(\d+):\s*([\d\.]+)\s*CS/s")

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
            os.path.join(self.base_dir, f"buckets_{kwargs['lock']}")
        )
        if exec_hash is None:
            raise Exception("Failed to hash executable.")

        return super().get_run_hash(exec_hash=exec_hash, kwargs=kwargs)

    def run(self, **kwargs):
        buckets_args = [f"--{k}={w}" for k, w in kwargs.items() if k != "lock"]

        commands = [
            os.path.join(self.base_dir, f"buckets_{kwargs['lock']}"),
            *buckets_args,
        ]
        print(" ".join(commands))

        est_runtime = self.estimate_runtime(**kwargs)
        try:
            result = subprocess.run(
                commands,
                capture_output=True,
                text=True,
                timeout=max(
                    10 / 1000 * est_runtime if est_runtime is not None else 60, 10
                ),
            )
            if result.returncode != 0:
                print(f"Failed to run buckets ({result.returncode}):", result.stderr)
                return None
        except Exception as e:
            print(f"Failed to run buckets:", e)
            return None

        results = {}
        for line in result.stdout.splitlines():
            if m := self.pattern_global.match(line):
                results["throughput"] = float(m.group(1))
                continue

            if m := self.pattern_local.match(line):
                results[f"throughput_t{int(m.group(1))}"] = float(m.group(2))

        return pd.DataFrame([results]) if results else None
