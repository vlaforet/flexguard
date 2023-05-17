#! /bin/bash

DELAY=10000
CORE_COUNT=40
MAX_THREAD_COUNT=45

LOCKS="HYBRIDLOCK MCS FUTEX"
CONTENTIONS="0 1000 1000000"
CACHELINES="1 5"

for lock in $LOCKS;
do
  echo "Starting ${lock} compilation"
  make LOCK_VERSION=-DUSE_HYBRIDLOCK_LOCKS NOBPF=1 clean all >/dev/null

  for contention in $CONTENTIONS;
  do
    for cacheline in $CACHELINES;
    do
      echo "Starting ${lock}_c${contention}_t${cacheline}"
      stdbuf -oL ./scheduling -n ${MAX_THREAD_COUNT} -s ${CORE_COUNT} -d ${DELAY} -c $contention -t $cacheline > "results/${lock}_c${contention}_t${cacheline}.csv"
    done;
  done;
done;