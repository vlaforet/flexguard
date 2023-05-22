Hybrid Lock
====

This is a fork of the [libslock project](https://github.com/tudordavid/libslock) with a new kind of lock that can automatically switch from spinning to blocking. This is done according to the current system load measured by detecting preemptions with ebpf.

## Installation

After cloning the repository update submodules:
```
git submodule update --init --recursive
```

Install required dependencies (mostly used to compile bpf programs).
On Ubuntu/Debian:
```
sudo apt install -y llvm clang libelf1 libelf-dev zlib1g-dev bpftrace
```

## Building
### Locks
There are several locks available in this library.
Any of these tags can be added to the make command to compile the desired version.

```
LOCK_VERSION=-DUSE_HCLH_LOCKS
LOCK_VERSION=-DUSE_TTAS_LOCKS
LOCK_VERSION=-DUSE_SPINLOCK_LOCKS
LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS // MCS/Futex hybrid lock
LOCK_VERSION=-DUSE_HYBRIDSPIN_LOCKS // Compare-And-Swap/Futex hybrid lock
LOCK_VERSION=-DUSE_MCS_LOCKS
LOCK_VERSION=-DUSE_ARRAY_LOCKS
LOCK_VERSION=-DUSE_RW_LOCKS
LOCK_VERSION=-DUSE_CLH_LOCKS
LOCK_VERSION=-DUSE_TICKET_LOCKS
LOCK_VERSION=-DUSE_MUTEX_LOCKS
LOCK_VERSION=-DUSE_FUTEX_LOCKS      // raw futex lock
LOCK_VERSION=-DUSE_HTICKET_LOCKS
```

### Getting Started
Binaries can be built from the root directory using

```
make LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS
```

### No BPF
If you do not need BPF (for a non-hybrid lock or when computing benchmarks) you can disable BPF entirely. This will significantly speed up compilation.
```
make LOCK_VERSION=-DUSE_FUTEX_LOCKS NOBPF=1
```

### Debug
When working on an implementation debug mode can help.
```
make LOCK_VERSION=-DUSE_MCS_LOCKS DEBUG=1
```


## Benchmarks
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
        Base number of threads (default=0)
  -c, --compute-cycles <int>
        Compute delay between critical sections, in cycles (default=100)
  -d, --launch-delay <int>
        Delay between thread creations in milliseconds (default=1000)
  -l, --use-locks <int>
        Use locks or not (default=1)
  -n, --num-threads <int>
        Number of threads (default=10)
  -t, --cache-lines <int>
        Number of cache lines touched in each CS (default=1)
  -s, --switch-thread-count <int>
        Core count after which the lock will be blocking (default=48)
        A value of -1 will disable the switch (always spin) and with a value of 0 the lock will never spin.
```

### Automated benchmarks
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
usage: chart_scheduling.py [-h] [-c CONTENTION [CONTENTION ...]]
                           [-t CACHE_LINE [CACHE_LINE ...]] [-l LOCK [LOCK ...]]
                           [-o OUTPUT_FILE]

Chart data from scheduling benchmark

optional arguments:
  -h, --help            show this help message and exit
  -c CONTENTION [CONTENTION ...]
                        Contention delays to show (default=[1000])
  -t CACHE_LINE [CACHE_LINE ...]
                        Cache lines to show (default=[1])
  -l LOCK [LOCK ...]    Locks to show (default=["hybrid_emulated", "futex", "mcs"])
  -o OUTPUT_FILE        Locks to show (default="out.png")
```

Example:
```
cd results
../scripts/chart_scheduling.py -c 0 -t 1 5 -l futex mcs
```
