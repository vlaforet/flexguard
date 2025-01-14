Hybrid Lock
====

This is a fork of the [libslock project](https://github.com/tudordavid/libslock) with a new kind of lock that can automatically switch from spinning to blocking. This is done according to the current system load measured by detecting preemptions with ebpf.

## Installation

Linux Kernel version > 6.1 should be used.

Install required dependencies (mostly used to compile bpf programs).
On Ubuntu/Debian:
```
apt-get update
apt-get install -y git pkg-config cmake software-properties-common build-essential python3-pip numactl python3-matplotlib libnuma-dev libelf1 libelf-dev zlib1g-dev bpftrace clang-16 papi-tools libpapi-dev
ln -s /usr/bin/clang-16 /usr/bin/clang
```

After cloning the repository update submodules:
```
git submodule update --init --recursive
```

## Building
### Getting Started
All binaries can be made at the same time thanks to the `make_all_versions.sh` script from the root directory.

```
./scripts/make_all_versions.sh [-v] [-s suffix] [-d] [-l locks]
    -v             verbose
    -s suffix      suffix the executable with suffix
    -d             delete
    -l locks       specify which lock(s) to make
```

Binaries can also be built separately using `make`. 

```
make LOCK_VERSION=HYBRIDLOCK
```

### Locks
There are several locks available in this library.
Any of these tags can be added to the make command to compile the desired version.

```
LOCK_VERSION=SPINLOCK
LOCK_VERSION=HYBRIDLOCK // Hybrid lock
LOCK_VERSION=FLEXGUARD   // Hybrid lock version 2
LOCK_VERSION=MCS
LOCK_VERSION=MCSTAS
LOCK_VERSION=CLH
LOCK_VERSION=TICKET
LOCK_VERSION=MUTEX
LOCK_VERSION=FUTEX      // Raw futex lock
```

### No BPF
If you do not need BPF for the Hybrid Lock (for example when some benchmarks) you can disable BPF entirely. This will significantly speed up compilation.
```
make LOCK_VERSION=HYBRIDLOCK NOBPF=1
```

### Debug
When working on an implementation debug mode can help.
```
make LOCK_VERSION=MCS DEBUG=1
```


## Microbenchmarks
### Scheduling
The `scheduling` benchmark has been tailored to test the hybrid lock.

```
scheduling -- lock stress test

Usage:
  scheduling [options...]

Options:
  -h, --help
        Print this message
  -b, --base-threads <int>
        Base number of threads (default=1)
  -c, --contention <int>
        Compute delay between critical sections, in cycles (default=100)
  -d, --step-duration <int>
        Duration of a step (measurement of a thread count) (default=1000)
  -n, --num-threads <int>
        Maximum number of threads (default=10)
  -t, --cache-lines <int>
        Number of cache lines touched in each CS (default=1)
  -s, --thread-step <int>
        A measurement will be taken every x thread step (default=1)
  -i, --increasing-only <int>
        Whether to increase then decrease or only increase thread count (default=1)
```

The `scripts/scheduling_all.sh` script can be run to compute a `scheduling` benchmark for different lock types (by default hybridlock, futex and mcs), different cache lines (by default 1 and 5) and different delays between critical sections (by default 0, 1000 and 1000000). The resulting CSVs will be placed in `results`.

Example:
```
./scripts/scheduling_all.sh
ls results

futex_c0_t1.csv        hybrid_emulated_c0_t1.csv        mcs_c0_t1.csv
futex_c0_t5.csv        hybrid_emulated_c0_t5.csv        mcs_c0_t5.csv
futex_c1000000_t1.csv  hybrid_emulated_c1000000_t1.csv  mcs_c1000000_t1.csv
futex_c1000000_t5.csv  hybrid_emulated_c1000000_t5.csv  mcs_c1000000_t5.csv
futex_c1000_t1.csv     hybrid_emulated_c1000_t1.csv     mcs_c1000_t1.csv
futex_c1000_t5.csv     hybrid_emulated_c1000_t5.csv     mcs_c1000_t5.csv
```

A Python script `scripts/chart_scheduling.py` is then available to create a graph from these CSVs.
```
usage: chart_scheduling.py [-h] [-i INPUT_FOLDER] [-c CONTENTION [CONTENTION ...]]
                           [-t CACHE_LINE [CACHE_LINE ...]] [-l LOCK [LOCK ...]] [-o OUTPUT_FILE]
                           [--increasing-only]

Chart data from scheduling benchmark

options:
  -h, --help            show this help message and exit
  -i INPUT_FOLDER       Input folder for CSVs (default="")
  -c CONTENTION [CONTENTION ...]
                        Contention delays to show (default=[1000])
  -t CACHE_LINE [CACHE_LINE ...]
                        Cache lines to show (default=[1])
  -l LOCK [LOCK ...]    Locks to show (default=["Futex", "Atomic_CLH", "Hybridlock"])
  -o OUTPUT_FILE        Locks to show (default="out.png")
  --increasing-only     Only show the increasing thread count part (default=False)
```

Example:
```
cd results
../scripts/chart_scheduling.py -c 0 -t 1 5 -l futex mcs
```

### Correctness benchmark
The `./scripts/test_correctness.sh` benchmark is made to test the correctness of all locks at the same time.

```
./scripts/test_correctness.sh [-v] [-s suffix] [-n num_cores] [-l locks]
    -d duration    test duration
    -s suffix      suffix the executable with suffix
    -n num_cores   number of cores to test on
    -l locks       specify which lock(s) to make
```

It will output a `correctness.out` file at the root of the repository containing the correctness results.

# LevelDB Benchmark
## Installation

```
git clone --recurse-submodules https://github.com/google/leveldb.git
cd leveldb && mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
```

## Build bpf-hybrid-locks with LiTL
In bpf-hybrid-locks root directory:
```
make all litl # Per-lock state
make all litl HYBRID_GLOBAL_STATE=1 # Global lock state
```

## Usage
The LevelDB benchmark can then be launched stock:
```
~/leveldb/build/db_bench --benchmarks=fillrandom,readrandom --threads=50 --num=100000 --db=/tmp/db
```

Or using bpf-hybrid-locks: (Must be root)
```
~/bpf-hybrid-locks/litl/liblibslock_original.sh ~/leveldb/build/db_bench --benchmarks=fillrandom,readrandom --threads=50 --num=100000 --db=/tmp/db
```

`--threads=50` specifies the number of database clients

`--num=100000` specifies the amount of operations done by each client