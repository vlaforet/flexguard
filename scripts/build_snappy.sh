#! /bin/bash

cd ext/snappy
mkdir -p build
cd build
cmake ../ && make
