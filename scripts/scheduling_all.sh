#! /bin/bash

LOCKS="HYBRIDV2 MCS MCSTAS MUTEX"
CONTENTIONS="0 1000 1000000"
CACHELINES="1 5"

DELAY=5000
THREAD_STEP=1
MAX_THREAD_COUNT=45

usage() {
  echo "$0 [-l locks] [-c contentions] [-t cachelines] [-d delay] [-s thread-step] [-n num-threads]"
  echo "    -l locks        specify which lock(s) to benchmark (default='$LOCKS')"
  echo "    -c contentions  specify which contentions level to test (default='$CONTENTIONS')"
  echo "    -t cachelines   specify which cache line counts to test (default='$CACHELINES')"
  echo "    -d delay        duration of a step (default='$DELAY')"
  echo "    -s thread-step  take a measurement every x threads (default='$THREAD_STEP')"
  echo "    -n num-threads  maximum number of threads (default='$MAX_THREAD_COUNT')"
}

while getopts "hl:c:t:d:s:n:" OPTION; do
  case $OPTION in
  h)
    usage
    exit 1
    ;;
  l)
    LOCKS="$OPTARG"
    echo "Benchmarking $LOCKS"
    ;;
  c)
    CONTENTIONS="$OPTARG"
    echo "Using contentions $CONTENTIONS"
    ;;
  t)
    CACHELINES="$OPTARG"
    echo "Using cache line counts $CACHELINES"
    ;;
  d)
    DELAY="$OPTARG"
    echo "Using delay $DELAY"
    ;;
  s)
    THREAD_STEP="$OPTARG"
    echo "Using thread step $THREAD_STEP"
    ;;
  n)
    MAX_THREAD_COUNT="$OPTARG"
    echo "Using max thread count $MAX_THREAD_COUNT"
    ;;

  ?)
    usage
    exit
    ;;
  esac
done

BASE_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
${BASE_DIR}/make_all_versions.sh -d -l "$LOCKS"
${BASE_DIR}/make_all_versions.sh -l "$LOCKS"

for lock in $LOCKS; do
  lock_name="${lock,,}"

  for contention in $CONTENTIONS; do
    for cacheline in $CACHELINES; do
      echo "Starting results/${lock_name}_c${contention}_t${cacheline}.csv"
      stdbuf -oL ./scheduling_${lock_name} -s ${THREAD_STEP} -n ${MAX_THREAD_COUNT} -d ${DELAY} -c $contention -t $cacheline >"results/${lock_name}_c${contention}_t${cacheline}.csv"
    done
  done
done
