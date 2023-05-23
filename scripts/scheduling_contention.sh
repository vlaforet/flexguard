#! /bin/bash

DELAY=100

LOCKS="FUTEX MCS HYBRIDLOCK"
CACHELINES="1 5"
THREADCOUNTS="30 40 50"

CONTENTIONS="0 1 2 3 5 6 7 9 12 15 19 25 31 39 50 63 79 100 125 158 199 251 316 398 501 630 794 1000 1258 1584 1995 2511 3162 3981 5011 6309 7943 10000 12589 15848 19952 25118 31622 39810 50118 63095 79432 99999 125892 158489 199526 251188 316227 398107 501187 630957 794328 999999 1258925 1584893"

for lock in $LOCKS;
do
  echo "Starting ${lock} compilation"
  make LOCK_VERSION=-DUSE_${lock}_LOCKS NOBPF=1 clean all >/dev/null

  for threadcount in $THREADCOUNTS;
  do
    for cacheline in $CACHELINES;
    do
      filename="results/contention_${lock,,}_t${cacheline}_n${threadcount}.csv"
      echo "Starting computation for $filename"
      echo "Duration: $DELAY" > $filename

      for contention in $CONTENTIONS;
      do
        avg=$(./scheduling -b $thread_count -n $threadcount -d ${DELAY} -c $contention -t $cacheline | grep "," | awk '{sum+=$3} END {print sum/NR}')
        echo "${contention}, ${avg}" >> $filename
      done;
    done;
  done;
done;