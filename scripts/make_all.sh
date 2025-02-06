#!/bin/bash

usage() {
    echo "$0 [-v] [-s suffix] [-d]"
    echo "    -v             verbose"
    echo "    -s suffix      suffix the executables with suffix"
    echo "    -d             debug mode"
}

USUFFIX=""
VERBOSE=0
DEBUG=0
while getopts "hs:vd" OPTION; do
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
    v)
        VERBOSE=1
        ;;
    d)
        DEBUG=1
        ;;
    ?)
        usage
        exit
        ;;
    esac
done

compile_and_suffix() {
    suffix="$1"
    cmd="make -j40 all DEBUG=${DEBUG} $2"

    make clean >/dev/null

    echo "Building $suffix: $cmd"
    if [ $VERBOSE -eq 1 ]; then
        eval $cmd
    else
        eval $cmd >/dev/null
    fi

    mv libsync.a libsync${suffix}${USUFFIX}.a
    mv buckets buckets_${suffix}${USUFFIX}
    mv test_correctness test_correctness_${suffix}${USUFFIX}
    mv test_init test_init_${suffix}${USUFFIX}
    mv scheduling scheduling_${suffix}${USUFFIX}
    mv interpose.sh interpose_${suffix}${USUFFIX}.sh
    mv interpose.so interpose_${suffix}${USUFFIX}.so
}

compile_and_suffix "mcstasnopad" "LOCK_VERSION=MCSTAS ADD_PADDING=0"
compile_and_suffix "mcstasextendnopad" "LOCK_VERSION=MCSTAS ADD_PADDING=0 TIMESLICE_EXTENSION=1"
compile_and_suffix "spinextendnopad" "LOCK_VERSION=SPINEXTEND ADD_PADDING=0"
compile_and_suffix "mcsextendnopad" "LOCK_VERSION=MCSEXTEND ADD_PADDING=0"
compile_and_suffix "mutexnopad" "LOCK_VERSION=MUTEX ADD_PADDING=0 USE_REAL_PTHREAD=1"
compile_and_suffix "futexnopad" "LOCK_VERSION=FUTEX ADD_PADDING=0"
compile_and_suffix "spinparknopad" "LOCK_VERSION=SPINPARK ADD_PADDING=0"
compile_and_suffix "mcsnopad" "LOCK_VERSION=MCS ADD_PADDING=0"
compile_and_suffix "mcstpnopad" "LOCK_VERSION=MCSTP ADD_PADDING=0 USE_REAL_PTHREAD=1"
compile_and_suffix "malthusiannopad" "LOCK_VERSION=MALTHUSIAN ADD_PADDING=0 USE_REAL_PTHREAD=1"
compile_and_suffix "shufflenopad" "LOCK_VERSION=SHUFFLE ADD_PADDING=0 USE_REAL_PTHREAD=1"
compile_and_suffix "shufflenoshufflenopad" "LOCK_VERSION=SHUFFLE ADD_PADDING=0 USE_REAL_PTHREAD=1 SHUFFLE_NO_SHUFFLE=1"
compile_and_suffix "shuffleextendnopad" "LOCK_VERSION=SHUFFLE ADD_PADDING=0 USE_REAL_PTHREAD=1 TIMESLICE_EXTENSION=1"
compile_and_suffix "mcsblocknopad" "LOCK_VERSION=MCSBLOCK ADD_PADDING=0"
compile_and_suffix "flexguardnopad" "LOCK_VERSION=FLEXGUARD ADD_PADDING=0"
compile_and_suffix "flexguardextendnopad" "LOCK_VERSION=FLEXGUARD ADD_PADDING=0 TIMESLICE_EXTENSION=1"

compile_and_suffix "mcstas" "LOCK_VERSION=MCSTAS ADD_PADDING=1"
compile_and_suffix "mcstasextend" "LOCK_VERSION=MCSTAS ADD_PADDING=1 TIMESLICE_EXTENSION=1"
compile_and_suffix "spinextend" "LOCK_VERSION=SPINEXTEND ADD_PADDING=1"
compile_and_suffix "mcsextend" "LOCK_VERSION=MCSEXTEND ADD_PADDING=1"
compile_and_suffix "mutex" "LOCK_VERSION=MUTEX ADD_PADDING=1 USE_REAL_PTHREAD=1"
compile_and_suffix "futex" "LOCK_VERSION=FUTEX ADD_PADDING=1"
compile_and_suffix "spinpark" "LOCK_VERSION=SPINPARK ADD_PADDING=1"
compile_and_suffix "mcs" "LOCK_VERSION=MCS ADD_PADDING=1"
compile_and_suffix "mcstp" "LOCK_VERSION=MCSTP ADD_PADDING=1 USE_REAL_PTHREAD=1"
compile_and_suffix "malthusian" "LOCK_VERSION=MALTHUSIAN ADD_PADDING=1 USE_REAL_PTHREAD=1"
compile_and_suffix "shuffle" "LOCK_VERSION=SHUFFLE ADD_PADDING=1 USE_REAL_PTHREAD=1"
compile_and_suffix "shufflenoshuffle" "LOCK_VERSION=SHUFFLE ADD_PADDING=1 USE_REAL_PTHREAD=1 SHUFFLE_NO_SHUFFLE=1"
compile_and_suffix "shuffleextend" "LOCK_VERSION=SHUFFLE ADD_PADDING=1 USE_REAL_PTHREAD=1 TIMESLICE_EXTENSION=1"
compile_and_suffix "mcsblock" "LOCK_VERSION=MCSBLOCK ADD_PADDING=1"
compile_and_suffix "flexguard" "LOCK_VERSION=FLEXGUARD ADD_PADDING=1"
compile_and_suffix "flexguardextend" "LOCK_VERSION=FLEXGUARD ADD_PADDING=1 TIMESLICE_EXTENSION=1"
