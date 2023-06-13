#! /bin/bash

DELAY=10000
CORE_COUNT=40
MAX_THREAD_COUNT=45

LOCKS="HYBRIDLOCK HYBRIDLOCK_NOBPF MCS FUTEX"
CONTENTIONS="0 1000 1000000"
CACHELINES="1 5"

for lock in $LOCKS;
do
    switch=""
    lock_name="${lock,,}"
    lock_flag="${lock}"
    bpf="NOBPF=1"

  if [ "$lock" == "HYBRIDLOCK_NOBPF" ]; then
    switch="-s ${CORE_COUNT}"
    lock_name="hybridlock_nobpf"
    lock_flag="HYBRIDLOCK"
  fi

  if [ "$lock" == "HYBRIDLOCK" ]; then
    bpf=""
  fi

  echo "Starting ${lock_name} compilation with flags switch=${switch}, bpf=${bpf}"
  make LOCK_VERSION=-DUSE_${lock_flag}_LOCKS ${bpf} clean all >/dev/null

  for contention in $CONTENTIONS;
  do
    for cacheline in $CACHELINES;
    do
      echo "Starting results/${lock_name}_c${contention}_t${cacheline}.csv"
      stdbuf -oL ./scheduling -n ${MAX_THREAD_COUNT} $switch -d ${DELAY} -c $contention -t $cacheline > "results/${lock_name}_c${contention}_t${cacheline}.csv"
    done;
  done;
done;