#!/bin/bash
LOCKS="SPINLOCK HYBRIDLOCK HYBRIDSPIN MCS CLH TICKET MUTEX FUTEX"

usage() {
  echo "$0 [-a args] [-s suffix] [-n num_cores] [-l locks] [-o output_csv] [-f] [-v]"
  echo "    -a args        buckets test argument"
  echo "    -s suffix      suffix the executable with suffix"
  echo "    -n num_cores   numbers of cores to test on"
  echo "    -l locks       specify which lock(s) to make"
  echo "    -o output_csv  output csv path"
  echo "    -f             ignore existing output csv"
  echo "    -v             verbose mode"
}

USUFFIX=""
NUM_CORES="10 30 40 50 70"
ARGS=""
OUTPUT="results/buckets.csv"
IGNORE_OUTPUT=0
VERBOSE=0
while getopts "ha:s:n:l:o:fv" OPTION; do
  case $OPTION in
  h)
    usage
    exit 1
    ;;
  a)
    ARGS="$OPTARG"
    ;;
  s)
    if [ -n "$OPTARG" ]; then
      USUFFIX="_$OPTARG"
      echo "Using suffix: $USUFFIX"
    fi
    ;;
  n)
    NUM_CORES="$OPTARG"
    ;;
  l)
    LOCKS="$OPTARG"
    ;;
  o)
    OUTPUT="$OPTARG"
    ;;
  f)
    IGNORE_OUTPUT=1
    ;;
  v)
    VERBOSE=1
    ;;
  ?)
    usage
    exit
    ;;
  esac
done

if [ "${IGNORE_OUTPUT}" -eq 0 ] && [ -f "$OUTPUT" ]; then
  echo 'Output csv already exists.'
  exit 1
fi
echo "Lock,Thread count,Throughput" >$OUTPUT

echo "Testing locks $LOCKS"
echo "Testing on ($NUM_CORES) cores"

BASE_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
${BASE_DIR}/make_all_versions.sh -d -l "$LOCKS" -s "$USUFFIX"
${BASE_DIR}/make_all_versions.sh -l "$LOCKS" -s "$USUFFIX"

for lock in $LOCKS; do
  for cores in $NUM_CORES; do
    suffix=$(echo $lock | tr "[:upper:]" "[:lower:]")
    echo "Benchmarking on lock ${suffix} with ${cores} threads"

    res="$(./buckets_$suffix$USUFFIX -n ${cores} ${ARGS})"
    throughput="$(echo "$res" | grep "Throughput" | awk '{print $2}')"

    echo "${lock},${cores},${throughput}" >>$OUTPUT

    if [ "${VERBOSE}" -eq 1 ]; then
      echo "$res"
    fi
  done
done
