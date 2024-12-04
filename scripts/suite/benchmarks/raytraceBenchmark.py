import os
import re
import subprocess
import sys

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class RaytraceBenchmark(BenchmarkCore):

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.raytrace_dir = os.path.join(self.base_dir, "ext", "raytrace")
        self.bin = os.path.join(self.raytrace_dir, "build/db_bench")
        self.bin = "/tmp/parsec-benchmark/ext/splash2x/apps/raytrace/inst/amd64-linux.gcc/bin/raytrace"

        self.pattern = re.compile(
            r"Total time without initialization\s+(?P<micros>\d+)"
        )

        if not os.path.isfile(self.bin):
            print("Raytrace binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("Raytrace binary permission denied")
            sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return None

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
                "/tmp/parsec-benchmark/ext/splash2x/apps/raytrace/run/balls4.env",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run Raytrace ({result.returncode}):", result.stderr)
            return None

        results = {}
        for match in self.pattern.finditer(result.stdout):
            results["time"] = match.group("micros")

        return pd.DataFrame([results]) if results else None
