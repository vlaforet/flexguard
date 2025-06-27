import os
import re
import subprocess
import sys

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import execute_command, sha256_hash_file


class VolrendBenchmark(BenchmarkCore):
    pattern = re.compile(r"Benchmark time:\s+(?P<micros>\d+)")

    input_file_hash = None

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.volrend_dir = os.path.join(
            self.base_dir, "ext/parsec-benchmark/ext/splash2x/apps/volrend"
        )
        self.run_dir = os.path.join(self.volrend_dir, "run")
        self.bin = os.path.join(self.volrend_dir, "inst/amd64-linux.gcc/bin/volrend")

        if not os.path.isfile(self.bin):
            print("Volrend binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("Volrend binary permission denied")
            sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return max(2000000 // kwargs.get("threads", 1), 15000)

    def get_run_hash(self, **kwargs):
        exec_hash = sha256_hash_file(self.bin)
        if exec_hash is None:
            raise Exception("Failed to hash executable.")

        interpose_hash = sha256_hash_file(
            os.path.join(self.base_dir, "build", f"interpose_{kwargs['lock']}.so")
        )
        if interpose_hash is None:
            raise Exception("Failed to hash interpose.so.")

        return super().get_run_hash(
            exec_hash=exec_hash,
            interpose_hash=interpose_hash,
            kwargs=kwargs,
        )

    def run(self, **kwargs):
        commands = [
            c
            for c in [
                (
                    os.path.join(
                        self.base_dir, "build", f"interpose_{kwargs['lock']}.sh"
                    )
                    if "lock" in kwargs and kwargs["lock"] != "stock"
                    else None
                ),
                self.bin,
                f"{kwargs.get('threads', 1)}",
                "head",
                "1000",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        try:
            returncode, stdout, stderr = execute_command(
                commands,
                timeout=8 / 1000 * self.estimate_runtime(**kwargs),
                kwargs={"cwd": self.run_dir},
            )
        except subprocess.TimeoutExpired as e:
            print(f"Volrend command timed out after {e.timeout} seconds")
            return None

        if returncode != 0:
            print(f"Failed to run Volrend ({returncode}):", stderr, stdout)
            return None

        results = {}
        for match in self.pattern.finditer(stdout):
            results["time"] = match.group("micros")

        return pd.DataFrame([results]) if results else None
