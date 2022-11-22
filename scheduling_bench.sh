#! /bin/bash

# hybrid lock
./scheduling $* > hybrid.csv

# Spinning (never switch to blocking)
./scheduling -s -1 $* > hybrid_spin.csv

# Futex (never spin)
./scheduling -s 0 $* > futex.csv

