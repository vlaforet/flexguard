#! /bin/bash

cd ext/index-benchmarks
mkdir -p build && cd build
sudo apt-get install -y libtbb-dev libgoogle-glog-dev
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j10