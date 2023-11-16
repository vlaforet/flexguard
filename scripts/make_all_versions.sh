#!/bin/sh

LOCKS="SPINLOCK HYBRIDLOCK HYBRIDSPIN MCS CLH TICKET MUTEX FUTEX"
BENCHMARKS="scheduling test_correctness"

MAKE="make"

usage() {
    echo "$0 [-v] [-s suffix] [-d] [-l locks]"
    echo "    -v             verbose"
    echo "    -s suffix      suffix the executable with suffix"
    echo "    -d             delete"
    echo "    -l locks       specify which lock(s) to make"
}

USUFFIX=""
VERBOSE=0
DELETE=0
while getopts "hs:vdl:" OPTION; do
    case $OPTION in
    h)
        usage
        exit 1
        ;;
    s)
        USUFFIX="_$OPTARG"
        echo "Using suffix: $USUFFIX"
        ;;
    v)
        VERBOSE=1
        ;;
    d)
        DELETE=1
        ;;
    l)
        LOCKS="$OPTARG"
        echo "Making $LOCKS"
        ;;
    ?)
        usage
        exit
        ;;
    esac
done

for lock in $LOCKS; do
    suffix=$(echo $lock | tr "[:upper:]" "[:lower:]")

    if [ $DELETE -eq 1 ]; then
        echo "Deleting: $lock"
        for benchmark in $BENCHMARKS; do
            rm ${benchmark}_$suffix$USUFFIX 2> /dev/null
        done
    else
        echo "Building: $lock"
        if [ $VERBOSE -eq 1 ]; then
            $MAKE all LOCK_VERSION=$lock
        else
            $MAKE all LOCK_VERSION=$lock >/dev/null
        fi

        for benchmark in $BENCHMARKS; do
            mv $benchmark ${benchmark}_$suffix$USUFFIX
        done
    fi
done
