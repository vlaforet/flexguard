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
compile_and_suffix "futexnopad" "LOCK_VERSION=FUTEX ADD_PADDING=0"
compile_and_suffix "bhlnopad" "LOCK_VERSION=HYBRIDV2 ADD_PADDING=0"
compile_and_suffix "mcstpnopad" "LOCK_VERSION=MCSTP ADD_PADDING=0"
compile_and_suffix "malthusiannopad" "USE_LITL_LIBRARY=malthusian_spinlock ADD_PADDING=0"
compile_and_suffix "concurrencynopad" "USE_LITL_LIBRARY=concurrency_original ADD_PADDING=0"
compile_and_suffix "shufflenopad" "USE_LITL_LIBRARY=aqmwonode_spin_then_park ADD_PADDING=0"
compile_and_suffix "cbomcsnopad" "USE_LITL_LIBRARY=cbomcs_spinlock ADD_PADDING=0"
#compile_and_suffix "bhllocalnopad" "LOCK_VERSION=HYBRIDV2 ADD_PADDING=0 HYBRIDV2_LOCAL_PREEMPTIONS=1"
#compile_and_suffix "bhlnextwaiterdetectionnopad" "LOCK_VERSION=HYBRIDV2 HYBRIDV2_NEXT_WAITER_DETECTION=2 ADD_PADDING=0"

compile_and_suffix "mcstas" "LOCK_VERSION=MCSTAS ADD_PADDING=1"
compile_and_suffix "spinextend" "LOCK_VERSION=SPINEXTEND ADD_PADDING=1"
compile_and_suffix "mutex" "LOCK_VERSION=MUTEX ADD_PADDING=1 USE_REAL_PTHREAD=1"
compile_and_suffix "futex" "LOCK_VERSION=FUTEX ADD_PADDING=1"
compile_and_suffix "spinpark" "LOCK_VERSION=SPINPARK ADD_PADDING=1"
compile_and_suffix "mcs" "LOCK_VERSION=MCS ADD_PADDING=1"
compile_and_suffix "mcstp" "LOCK_VERSION=MCSTP ADD_PADDING=1"
compile_and_suffix "malthusian" "USE_LITL_LIBRARY=malthusian_spinlock ADD_PADDING=1"
cp litl/libmalthusian_spinlock.sh interpose_malthusian.sh
compile_and_suffix "concurrency" "USE_LITL_LIBRARY=concurrency_original ADD_PADDING=1"
cp litl/libconcurrency_original.sh interpose_concurrency.sh
compile_and_suffix "shuffle" "USE_LITL_LIBRARY=aqmwonode_spin_then_park ADD_PADDING=1"
cp litl/libaqmwonode_spin_then_park.sh interpose_shuffle.sh
compile_and_suffix "mcslitl" "USE_LITL_LIBRARY=mcs_spinlock ADD_PADDING=1"
cp litl/libmcs_spinlock.sh interpose_mcslitl.sh
compile_and_suffix "cbomcs" "USE_LITL_LIBRARY=cbomcs_spinlock ADD_PADDING=1"
cp litl/libcbomcs_spinlock.sh interpose_cbomcs.sh
compile_and_suffix "mcsblock" "LOCK_VERSION=MCSBLOCK ADD_PADDING=1"
compile_and_suffix "hybridv2" "LOCK_VERSION=HYBRIDV2 ADD_PADDING=1"
#compile_and_suffix "hybridv2local" "LOCK_VERSION=HYBRIDV2 ADD_PADDING=1 HYBRIDV2_LOCAL_PREEMPTIONS=1"
#compile_and_suffix "hybridv2nextwaiterdetection" "LOCK_VERSION=HYBRIDV2 HYBRIDV2_NEXT_WAITER_DETECTION=2 ADD_PADDING=1"

make -C litl >/dev/null
