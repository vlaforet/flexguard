import os
import re
import subprocess
import sys
import uuid

import pandas as pd
from benchmarks.benchmarkCore import BenchmarkCore
from utils import sha256_hash_file


class DedupBenchmark(BenchmarkCore):
    setup_pattern = re.compile(r"Setup time:\s+(\d+)")
    run_pattern = re.compile(r"Benchmark time:\s+(\d+)")

    input_file_hash = None

    def __init__(self, base_dir, temp_dir):
        super().__init__(base_dir, temp_dir)
        self.parsec_dir = os.path.join(self.base_dir, "ext", "parsec-benchmark")
        self.input_file = os.path.join(
            self.parsec_dir, "pkgs/kernels/dedup/run", "FC-6-x86_64-disc1.iso"
        )
        self.bin = os.path.join(
            self.parsec_dir, "pkgs/kernels/dedup/inst/amd64-linux.gcc/bin/dedup"
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

        if not os.path.isfile(self.input_file):
            print("Dedup input file not found")
            sys.exit(1)
        if not os.path.isfile(self.bin):
            print("Dedup binary not found")
            sys.exit(1)
        if not os.access(self.bin, os.R_OK | os.W_OK | os.X_OK):
            print("Dedup binary permission denied")
            sys.exit(1)

    def init(self, **kwargs):
        self.compressed_output_path = os.path.join(
            self.temp_dir, f"{uuid.uuid4()}.dat.ddp"
        )

        if "compress" in kwargs and not kwargs["compress"]:
            self.uncompressed_output_path = os.path.join(
                self.temp_dir, f"{uuid.uuid4()}.iso"
            )

            try:
                subprocess.run(
                    [
                        c
                        for c in [
                            self.bin,
                            "-c",
                            "-p",
                            (
                                f"-w{kwargs['compression_type']}"
                                if "compression_type" in kwargs
                                else None
                            ),
                            f"-t20",
                            f"-i{self.input_file}",
                            f"-o{self.compressed_output_path}",
                        ]
                    ],
                    capture_output=True,
                    text=True,
                    timeout=100,
                )
            except Exception as e:
                print(f"Failed to init dedup:", e)
                sys.exit(1)

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
        is_compress = "compress" not in kwargs or kwargs["compress"]

        commands = [
            c
            for c in [
                (
                    os.path.join(self.base_dir, f"interpose_{kwargs['lock']}.sh")
                    if "lock" in kwargs and kwargs["lock"] != "stock"
                    else None
                ),
                self.bin,
                "-c" if is_compress else "-u",
                "-p",
                (
                    f"-w{kwargs['compression_type']}"
                    if "compression_type" in kwargs
                    else None
                ),
                f"-t{kwargs['threads']}" if "threads" in kwargs else None,
                f"-i{self.input_file if is_compress else self.compressed_output_path}",
                f"-o{self.compressed_output_path if is_compress else self.uncompressed_output_path}",
            ]
            if c is not None
        ]
        print(" ".join(commands))

        try:
            result = subprocess.run(
                commands,
                capture_output=True,
                text=True,
                timeout=100 / 1000 * self.estimate_runtime(**kwargs),
            )
            if result.returncode != 0:
                print(result.stdout)
                print(f"Failed to run dedup ({result.returncode}):", result.stderr)
        except Exception as e:
            print(f"Failed to run dedup:", e)

        try:
            os.remove(self.compressed_output_path)
            os.remove(self.uncompressed_output_path)
        except:
            pass

        results = {}
        for line in result.stdout.splitlines():
            if m := self.setup_pattern.match(line):
                results["setup_time"] = int(m.group(1)) / self.frequency
                continue

            if m := self.run_pattern.match(line):
                results["run_time"] = int(m.group(1)) / self.frequency

        return pd.DataFrame([results]) if results else None
