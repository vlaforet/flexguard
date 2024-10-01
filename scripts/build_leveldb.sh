#! /bin/bash

cd ext/leveldb
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
