#!/bin/bash
LOCKS="SPINLOCK HYBRIDLOCK MCS CLH TICKET MUTEX FUTEX"

usage() {
  echo "$0 [-v] [-s suffix] [-n num_cores] [-l locks]"
  echo "    -d duration    test duration"
  echo "    -s suffix      suffix the executable with suffix"
  echo "    -n num_cores   number of cores to test on"
  echo "    -l locks       specify which lock(s) to make"
}

USUFFIX=""
DURATION=1000
NUM_CORES=8
while getopts "hs:d:n:l:" OPTION; do
  case $OPTION in
  h)
    usage
    exit 1
    ;;
  s)
    USUFFIX="_$OPTARG"
    echo "Using suffix: $USUFFIX"
    ;;
  d)
    DURATION="$OPTARG"
    ;;
  n)
    NUM_CORES="$OPTARG"
    ;;
  l)
    LOCKS="$OPTARG"
    echo "Testing $LOCKS"
    ;;
  ?)
    usage
    exit
    ;;
  esac
done

echo "Testing on $NUM_CORES cores"
echo "Testing for $DURATION ms"

rm correctness_${USUFFIX}.out 2> /dev/null

BASE_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
${BASE_DIR}/make_all_versions.sh -d -l "$LOCKS"
${BASE_DIR}/make_all_versions.sh -l "$LOCKS"

for lock in $LOCKS; do
  suffix=$(echo $lock | tr "[:upper:]" "[:lower:]")
  echo ${suffix} >>correctness_${USUFFIX}.out

  ./test_correctness_$suffix$USUFFIX -n ${NUM_CORES} -d ${DURATION} >>correctness_${USUFFIX}.out
done
