#! /bin/bash

cd ext/leveldb-1.20
# cd jemalloc/
# ./autogen.sh
# ./configure --without-export --disable-libdl
# make -j$(nproc)
# cd ../

./build_detect_platform build_config.mk .
make -j$(nproc)
