#! /bin/bash

make clean
make LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS

# hybrid lock
./scheduling $* | tee hybrid.csv

# Spinning (never switch to blocking)
./scheduling -s -1 $* | tee hybrid_spin.csv

# Futex (never spin)
./scheduling -s 0 $* | tee futex.csv

make clean
make LOCK_VERSION=-DUSE_SPINLOCK_LOCKS

# Spinning
./scheduling $* | tee spin.csv

