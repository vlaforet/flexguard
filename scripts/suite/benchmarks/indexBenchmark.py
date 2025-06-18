import os
import re
import subprocess
import sys

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import execute_command, sha256_hash_file


class IndexBenchmark(BenchmarkCore):
    opTypes = ["Insert", "Read", "Update", "Remove", "Scan"]
    finishTypes = ["completed", "succeeded"]

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
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

    def estimate_runtime(self, **kwargs):
        if "mode" in kwargs and kwargs["mode"] == "time":
            return kwargs["seconds"] * 1000 if "seconds" in kwargs else 20000
        return 3000

    def get_run_hash(self, **kwargs):
        exec_hash = sha256_hash_file(
            os.path.join(
                self.index_dir, f"build/wrappers/lib{kwargs['index']}_wrapper.so"
            )
        )
        if exec_hash is None:
            raise Exception("Failed to hash libwrapper.so.")

        pibench_hash = sha256_hash_file(self.pibench_bin)
        if pibench_hash is None:
            raise Exception("Failed to hash PiBench.")

        return super().get_run_hash(
            exec_hash=exec_hash, pibench_hash=pibench_hash, kwargs=kwargs
        )

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

        try:
            returncode, stdout, stderr = execute_command(
                commands,
                timeout=60 / 1000 * self.estimate_runtime(**kwargs),
            )
        except subprocess.TimeoutExpired as e:
            print(f"PiBench command timed out after {e.timeout} seconds")
            return None

        if returncode != 0:
            print(f"Failed to run PiBench ({returncode}):", stderr, stdout)
            return None

        results = {}
        for line in stdout.splitlines():
            for col, pattern in self.patterns.items():
                if m := pattern.match(line):
                    results[col] = float(m.group(1))
                    break

        for finish_type in self.finishTypes:
            results[finish_type] = sum(
                results.get(f"{opType}-{finish_type}", 0) for opType in self.opTypes
            )

        return pd.DataFrame([results]) if results else None
