#!/bin/bash

make clean >/dev/null
make -j40 all LOCK_VERSION=MUTEX USE_REAL_PTHREAD=1 TEST_INTERPOSE=1
make test_interpose

./interpose.sh ./test_interpose
