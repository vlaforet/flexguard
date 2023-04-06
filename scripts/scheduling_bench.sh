#! /bin/bash

DELAY=1000
CORE_COUNT=40
MAX_THREAD_COUNT=55

cd ..
rm -r tmp
mkdir tmp

######  Hybrid Lock
echo "Starting Hybrid Lock compilation"
make clean >/dev/null 2>/dev/null
make LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS >/dev/null 2>/dev/null

echo "Starting Hybrid Lock computation"
sudo ./scheduling -n ${MAX_THREAD_COUNT} -s -1 -d ${DELAY} > tmp/hybrid.csv

######  Hybrid Lock, BPF disabled
echo "Starting Hybrid Lock (No BPF) compilation"
make clean >/dev/null 2>/dev/null
make LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS NOBPF=1 >/dev/null 2>/dev/null

echo "Starting Hybrid Lock (No BPF) emulated computation"
./scheduling -n $MAX_THREAD_COUNT -s ${CORE_COUNT} -d ${DELAY} > tmp/hybrid_emulated.csv

echo "Starting Hybrid Lock (No BPF) spin computation"
./scheduling -n ${MAX_THREAD_COUNT} -s -1 -d ${DELAY} > tmp/hybrid_spin.csv

echo "Starting Hybrid Lock (No BPF) blocking computation"
./scheduling -n ${MAX_THREAD_COUNT} -s 0 -d ${DELAY} > tmp/hybrid_blocking.csv

###### MCS lock
echo "Starting MCS compilation"
make clean >/dev/null 2>/dev/null
make LOCK_VERSION=-DUSE_MCS_LOCKS NOBPF=1 >/dev/null 2>/dev/null

echo "Starting MCS computation"
./scheduling -n ${MAX_THREAD_COUNT} -d ${DELAY} > tmp/mcs.csv

###### Mutex lock
echo "Starting Mutex compilation"
make clean >/dev/null 2>/dev/null
make LOCK_VERSION=-DUSE_MUTEX_LOCKS NOBPF=1 >/dev/null 2>/dev/null

echo "Starting Mutex computation"
./scheduling -n ${MAX_THREAD_COUNT} -d ${DELAY} > tmp/mutex.csv

###### Spin lock
echo "Starting Spin Lock compilation"
make clean >/dev/null 2>/dev/null
make LOCK_VERSION=-DUSE_SPINLOCK_LOCKS NOBPF=1 >/dev/null 2>/dev/null

echo "Starting Spin Lock computation"
./scheduling -n ${MAX_THREAD_COUNT} -d ${DELAY} > tmp/spin.csv