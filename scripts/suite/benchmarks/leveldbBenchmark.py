import os
import re
import subprocess
import sys
import uuid

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore


class LevelDBBenchmark(BenchmarkCore):

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.leveldb_dir = os.path.join(self.base_dir, "ext", "leveldb")
        self.bin = os.path.join(self.leveldb_dir, "build/db_bench")

        self.pattern = re.compile(
            r"(?P<name>\w+)\s+:\s+(?P<micros>\d+\.\d+)\s+micros/op;"
        )

        if not os.path.isfile(self.bin):
            print("LevelDB binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("LevelDB binary permission denied")
            sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return None

    def run(self, **kwargs):
        args = [
            f"--{k}={w}" for k, w in kwargs.items() if k != "lock" and k != "benchmarks"
        ]

        if "benchmarks" in kwargs:
            args.append(f"--benchmarks={','.join(kwargs['benchmarks'])}")

        db_path = os.path.join(self.temp_dir, f"{uuid.uuid4()}.db")
        commands = [
            c
            for c in [
                (
                    os.path.join(self.base_dir, f"interpose_{kwargs['lock']}.sh")
                    if "lock" in kwargs and kwargs["lock"] != "stock"
                    else None
                ),
                self.bin,
                *args,
                f"--db={db_path}",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        result = subprocess.run(commands, capture_output=True, text=True)
        if result.returncode != 0:
            print(f"Failed to run LevelDB ({result.returncode}):", result.stderr)
            return None

        try:
            os.remove(db_path)
        except:
            pass

        results = {}
        for match in self.pattern.finditer(result.stdout):
            micros = match.group("micros")
            base_name = f"latency_{match.group('name')}"
            name = base_name

            count = 1
            while name in results:
                count += 1
                name = f"{base_name}_{count}"

            results[name] = micros

        return pd.DataFrame([results]) if results else None
