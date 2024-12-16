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

    def init(self, **kwargs):
        self.db_path = os.path.join(self.temp_dir, f"{uuid.uuid4()}.db")

        try:
            subprocess.run(
                [
                    self.bin,
                    f"--db={self.db_path}",
                    "--benchmarks=fillseq",
                    "--threads=1",
                ],
                capture_output=True,
                text=True,
                timeout=30,
            )
        except Exception as e:
            print(f"Failed to init LevelDB:", e)
            sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return kwargs["time_ms"] if "time_ms" in kwargs else 30000

    def run(self, **kwargs):
        args = [
            f"--{k}={w}"
            for k, w in kwargs.items()
            if k not in ["lock", "benchmarks", "init_db", "use_existing_db"]
        ]

        if "benchmarks" in kwargs:
            args.append(f"--benchmarks={','.join(kwargs['benchmarks'])}")

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
                f"--db={self.db_path}",
                "--use_existing_db=1",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        try:
            result = subprocess.run(
                commands,
                capture_output=True,
                text=True,
                timeout=1.5 / 1000 * self.estimate_runtime(**kwargs),
            )
            if result.returncode != 0:
                print(f"Failed to run LevelDB ({result.returncode}):", result.stderr)
        except Exception as e:
            print(f"Failed to run LevelDB:", e)

        try:
            os.remove(self.db_path)
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
