import os
import re
import subprocess
import sys

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import sha256_hash_file


class RaytraceBenchmark(BenchmarkCore):
    pattern = re.compile(r"Total time without initialization\s+(?P<micros>\d+)")

    input_file_hash = None

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.raytrace_dir = os.path.join(
            self.base_dir, "ext/parsec-benchmark/ext/splash2x/apps/raytrace"
        )
        self.input_file = os.path.join(self.raytrace_dir, "run", "car.env")
        self.bin = os.path.join(self.raytrace_dir, "inst/amd64-linux.gcc/bin/raytrace")

        if not os.path.isfile(self.input_file):
            print("Raytrace input file not found")
            sys.exit(1)
        if not os.path.isfile(self.bin):
            print("Raytrace binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("Raytrace binary permission denied")
            sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return 1000

    def get_run_hash(self, **kwargs):
        exec_hash = sha256_hash_file(self.bin)
        if exec_hash is None:
            raise Exception("Failed to hash executable.")

        interpose_hash = sha256_hash_file(
            os.path.join(self.base_dir, f"interpose_{kwargs['lock']}.so")
        )
        if interpose_hash is None:
            raise Exception("Failed to hash interpose.so.")

        if self.input_file_hash is None:
            self.input_file_hash = sha256_hash_file(self.input_file)
            if self.input_file_hash is None:
                raise Exception("Failed to hash input file.")

        return super().get_run_hash(
            exec_hash=exec_hash,
            interpose_hash=interpose_hash,
            input_file_hash=self.input_file_hash,
            kwargs=kwargs,
        )

    def run(self, **kwargs):
        commands = [
            c
            for c in [
                (
                    os.path.join(self.base_dir, f"interpose_{kwargs['lock']}.sh")
                    if "lock" in kwargs and kwargs["lock"] != "stock"
                    else None
                ),
                self.bin,
                f"-p{kwargs['threads']}",
                "-a8",
                self.input_file,
            ]
            if c is not None
        ]
        print(" ".join(commands))

        result = subprocess.run(
            commands,
            capture_output=True,
            text=True,
            cwd=os.path.dirname(self.input_file),
            timeout=100 / 1000 * self.estimate_runtime(**kwargs),
        )
        if result.returncode != 0:
            print(f"Failed to run Raytrace ({result.returncode}):", result.stderr)
            return None

        results = {}
        for match in self.pattern.finditer(result.stdout):
            results["time"] = match.group("micros")

        return pd.DataFrame([results]) if results else None
