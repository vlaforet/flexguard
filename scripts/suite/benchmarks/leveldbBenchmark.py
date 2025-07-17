import os
import re
import subprocess
import sys
import uuid

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import execute_command, sha256_hash_file


class LevelDBBenchmark(BenchmarkCore):

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.leveldb_dir = os.path.join(self.base_dir, "ext", "leveldb-1.20")
        self.bin = os.path.join(self.leveldb_dir, "out-static/db_bench")

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

        if "init_db" in kwargs and kwargs["init_db"]:
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
                print("Failed to init LevelDB:", e)
                sys.exit(1)

    def estimate_runtime(self, **kwargs):
        return kwargs["time_ms"] if "time_ms" in kwargs else 30000

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
            exec_hash=exec_hash, interpose_hash=interpose_hash, kwargs=kwargs
        )

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
                    os.path.join(
                        self.base_dir, "build", f"interpose_{kwargs['lock']}.sh"
                    )
                    if "lock" in kwargs and kwargs["lock"] != "stock"
                    else None
                ),
                self.bin,
                *args,
                f"--db={self.db_path}",
                f"--use_existing_db={'1' if 'init_db' in kwargs and kwargs['init_db'] else '0'}",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        try:
            returncode, stdout, stderr = execute_command(
                commands,
                timeout=1.5 / 1000 * self.estimate_runtime(**kwargs),
            )
        except subprocess.TimeoutExpired as e:
            print(f"LevelDB command timed out after {e.timeout} seconds")
            return None

        if returncode != 0:
            print(f"Failed to run LevelDB ({returncode}):", stderr, stdout)
            return None

        try:
            os.remove(self.db_path)
        except:
            pass

        results = {}
        for match in self.pattern.finditer(stdout):
            micros = match.group("micros")
            base_name = f"latency_{match.group('name')}"
            name = base_name

            count = 1
            while name in results:
                count += 1
                name = f"{base_name}_{count}"

            results[name] = micros

        return pd.DataFrame([results]) if results else None
