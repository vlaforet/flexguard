import os
import re
import subprocess
import sys
import uuid

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import execute_command, sha256_hash_file


class StreamclusterBenchmark(BenchmarkCore):
    setup_pattern = re.compile(r"Setup time:\s+(\d+)")
    run_pattern = re.compile(r"Benchmark time:\s+(\d+)")

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.parsec_dir = os.path.join(self.base_dir, "ext", "parsec-benchmark")
        self.bin = os.path.join(
            self.parsec_dir,
            "pkgs/kernels/streamcluster/inst/amd64-linux.gcc/bin/streamcluster",
        )

        result = subprocess.run(
            'sudo bpftrace -e \'BEGIN { printf("%u", *kaddr("tsc_khz")); exit(); }\' | sed -n 2p',
            shell=True,
            capture_output=True,
            text=True,
            timeout=10,
        )
        self.frequency = int(result.stdout.strip())
        if self.frequency == 0:
            print(
                "Failed to get CPU frequency. Check that bpftrace is properly installed."
            )
            sys.exit(1)

        if not os.path.isfile(self.bin):
            print("Streamcluster binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("Streamcluster binary permission denied")
            sys.exit(1)

    def init(self, **kwargs):
        self.output_path = os.path.join(self.temp_dir, f"{uuid.uuid4()}.txt")

    def estimate_runtime(self, **kwargs):
        if "threads" in kwargs:
            if kwargs["threads"] == 1:
                return 32000
            if kwargs["threads"] <= 8:
                return 2 * 32000 / kwargs["threads"]
        return 15000

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
        commands = [
            str(c)
            for c in [
                (
                    os.path.join(
                        self.base_dir, "build", f"interpose_{kwargs['lock']}.sh"
                    )
                    if "lock" in kwargs and kwargs["lock"] != "stock"
                    else None
                ),
                self.bin,
                kwargs.get("min_centers", 10),
                kwargs.get("max_centers", 30),
                kwargs.get("dimensions", 512),
                kwargs.get("num_points", 32768),
                kwargs.get("chunksize", 32768),
                kwargs.get("clustersize", 2000),
                "none",  # Input file (if n<=0)
                self.output_path,
                kwargs.get("threads", 10),
            ]
            if c is not None
        ]
        print(" ".join(commands))

        try:
            returncode, stdout, stderr = execute_command(
                commands,
                timeout=5 / 1000 * self.estimate_runtime(**kwargs),
            )
        except subprocess.TimeoutExpired as e:
            print(f"Streamcluster command timed out after {e.timeout} seconds")
            return None

        if returncode != 0:
            print(f"Failed to run Streamcluster ({returncode}):", stderr, stdout)
            return None

        try:
            os.remove(self.output_path)
        except:
            pass

        results = {}
        for line in stdout.splitlines():
            if m := self.setup_pattern.match(line):
                results["setup_time"] = int(m.group(1)) / self.frequency
                continue

            if m := self.run_pattern.match(line):
                results["run_time"] = int(m.group(1)) / self.frequency

        return pd.DataFrame([results]) if results else None
