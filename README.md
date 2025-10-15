FlexGuard
====

Performance-oriented applications require efficient locks to harness the computing power of multicore architectures. While fast, spinlock algorithms suffer severe performance degradation when thread counts exceed available hardware capacity, i.e., in oversubscribed scenarios. Existing solutions rely on imprecise heuristics for blocking, leading to suboptimal performance. We present FlexGuard, the first approach that systematically switches from busy-waiting to blocking precisely when a lock-holding thread is preempted. FlexGuard achieves this by communicating with the OS scheduler via eBPF, unlike prior approaches. FlexGuard matches or improves performance in LevelDB, a memory-optimized database index, PARSEC's Dedup, and SPLASH2X's Raytrace and Streamcluster, boosting throughput by 1-6x in non-oversubscribed and up to 5x in oversubscribed scenarios.

## Source code

FlexGuard's source code is available in these files:

- **src/flexguard.c** User-space code, contains `lock()`/`unlock()` functions.
- **src/flexguard.bpf.c** *eBPF* code, contains the `sched_switch` event handler of the critical section preemption monitor.
- **include/flexguard_bpf.h** Shared header between *eBPF* and user-space code. Contains type definitions for MCS qnodes.
- **include/flexguard.h** User-space header, contains user-space type definitions, and functions declarations.


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

## Getting Started
### Building
All lock binaries can be built at the same time using the `./scripts/make_all.sh` script:

```
./scripts/make_all.sh [-v] [-s suffix] [-d]
    -v             verbose
    -s suffix      suffix the executables with suffix
    -d             debug mode
    -p             enable pause counter
    -w waitmode    set condvars wait mode (AUTO, BLOCK, SPIN). Default: BLOCK
```

To build all default versions use
```
./scripts/make_all.sh
```

Built files will be located in `build/`.

### (Optional) Building benchmarks
Benchmarks used in the paper can be built using the `./scripts/build_*.sh` scripts. For example, build LevelDB with
```
./scripts/build_leveldb-1.20.sh
```

### Usage
The `build/` directory contains microbenchmark binaries for each lock versions as described in the next section as well as interposition helpers using `LD_PRELOAD` to replace all POSIX `pthread` locks (`mutex`, `rwlock`) by a specific lock implementation.

For example, to use FlexGuard on LevelDB (requires root):
```
./build/interpose_flexguard.sh ./ext/leveldb-1.20/out-static/db_bench --benchmarks=readrandom --threads=50 --num=100000 --db=/tmp/flexguard-level.db
```

and to compare it with a standard POSIX mutex lock:
```
./build/interpose_mutex.sh ./ext/leveldb-1.20/out-static/db_bench --benchmarks=readrandom --threads=50 --num=100000 --db=/tmp/mutex-level.db
```

## Microbenchmarks
### Single-lock shared variable microbenchmark
The `scheduling` benchmark has been tailored to test FlexGuard.

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
  -l, --latency <int>
        If true, measure cs latency else measure total throughput (default=0)
  -m, --multi-locks <int>
        How many locks to use (default=1)
```

### Hash table benchmark
The `buckets` benchmark creates 100 hash table buckets, each bucket protected using a lock. The buckets are uniformly spread across the value range. `N` threads are created a insert/get keys from the hash table using a skewed Zipfian distribution.

```
buckets -- lock stress test

Usage:
  buckets [options...]

Options:
  -h, --help
        Print this message
  -d, --duration <int>
        Duration of the experiment in ms (default=10000)
  -n, --num-threads <int>
        Maximum number of threads (default=10)
  -b, --buckets <int>
        Number of buckets (default=100)
  -m, --max-value <int>
        Maximum value (default=100000)
  -o, --offset-changes <int>
        Number of time to change the offset (default=40)
  -c, --non-critical-cycles <int>
        Number of cycles between critical sections (default=0)
  -p, --pin-threads <int>
        Enable thread pinning (default=0)
  -t, --trace
        Enable tracing (default=0)
        Lock tracing is disabled. If you use that option only the benchmark will be traced.
        Recompile with TRACING=1 to enable lock tracing.
```