Hybrid Lock
====

This is a fork of the [libslock project](https://github.com/tudordavid/libslock) with a new kind of lock that can automatically switch from spinning to blocking. This is done according to the current system load measured by preemptions with ebpf.

## Installation

After cloning the repository update submodules:
```
git submodule update --init --recursive
```

Install required dependencies (mostly used to compile bpf programs).
On Ubuntu/Debian:
```
sudo apt install -y llvm clang libelf1 libelf-dev zlib1g-dev
```

## Benchmarks
First, compile the library from the root directory:
```
make
```

The `scheduling` benchmark has been tailored to test the hybrid lock.

## BPF tests
For the moment, bpf hasn't been integrated into the lock. Code that is close to what will be implemented later is available in `libbpg-test`.

The code can be compiled with `make` in the directory.

Running `sudo ./sched_switch` will print preemptions of the main thread of this program.


