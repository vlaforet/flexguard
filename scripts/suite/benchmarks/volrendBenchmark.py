import os
import re
import subprocess
import sys
from timeit import timeit

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class VolrendBenchmark(BenchmarkCore):

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.volrend_dir = os.path.join(self.base_dir, "ext", "volrend")
        self.bin = os.path.join(self.volrend_dir, "build/db_bench")
        self.bin = "/tmp/parsec-benchmark/ext/splash2x/apps/volrend/inst/amd64-linux.gcc/bin/volrend"

        self.pattern = re.compile(
            r"(?P<name>\w+)\s+:\s+(?P<micros>\d+\.\d+)\s+micros/op;"
        )

        if not os.path.isfile(self.bin):
            print("Volrend binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("Volrend binary permission denied")
            sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return None

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
                str(kwargs["threads"] if "threads" in kwargs else 10),
                "/tmp/parsec-benchmark/ext/splash2x/apps/volrend/run/head-scaleddown2",
                "100",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        elapsed = timeit(
            lambda: subprocess.run(commands, capture_output=True, text=True), number=1
        )

        return pd.DataFrame([{"time": elapsed}])

        # if result.returncode != 0:
        #    print(f"Failed to run Volrend ({result.returncode}):", result.stderr)
        #    return None


#
# print(result)
# return
#
# results = {}
# for match in self.pattern.finditer(result.stdout):
#    micros = match.group("micros")
#    base_name = f"latency_{match.group('name')}"
#    name = base_name
#
#    count = 1
#    while name in results:
#        count += 1
#        name = f"{base_name}_{count}"
#
#    results[name] = micros
#
# return pd.DataFrame([results]) if results else None
