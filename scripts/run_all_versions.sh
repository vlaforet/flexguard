#!/bin/bash

usage() {
    echo "$0 [-s suffix] [-d] [-l locks]"
    echo "    -s suffix      suffix the executable with suffix"
    echo "    -l locks       specify which lock(s) to run"
    echo "    -b             benchmark"
    echo "    -a args        benchmark arguments"
}

LOCKS="HYBRIDLOCK MCS CLH TICKET MUTEX FUTEX"
USUFFIX=""
BENCHMARK="buckets"
ARGS=""
while getopts "hs:l:b:a:" OPTION; do
    case $OPTION in
    h)
        usage
        exit 1
        ;;
    s)
        if [ -n "$OPTARG" ]; then
            USUFFIX="_$OPTARG"
            echo "Using suffix: $USUFFIX"
        fi
        ;;
    l)
        LOCKS="$OPTARG"
        echo "Making $LOCKS"
        ;;
    b)
        BENCHMARK="$OPTARG"
        echo "Running benchmark $BENCHMARK"
        ;;
    a)
        ARGS="$OPTARG"
        echo "Using benchmark arguments $ARGS"
        ;;
    ?)
        usage
        exit
        ;;
    esac
done

for l in $LOCKS; do
    suffix=$(echo $l | tr "[:upper:]" "[:lower:]")

    echo "Benchmarking lock ${suffix}"
    ./${BENCHMARK}_${suffix}${USUFFIX} $ARGS
done
