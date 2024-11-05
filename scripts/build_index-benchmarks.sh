#! /bin/bash
set -e

if [ "$1" == "clean" ]; then
  rm -rf ext/index-benchmarks/build
  shift
fi

if [ "$1" == "debug" ]; then
  BUILDMODE="Debug"
  DEBUG=1
else
  BUILDMODE="Release"
  DEBUG=0
fi

cd ext/index-benchmarks
mkdir -p build && cd build
sudo apt-get install -y libtbb-dev libgoogle-glog-dev
cmake -DCMAKE_BUILD_TYPE=${BUILDMODE} ..
make -j40
