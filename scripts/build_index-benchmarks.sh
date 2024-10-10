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

make clean
make -j40 libsync.a LOCK_VERSION=MCSTAS ADD_PADDING=0 DEBUG=${DEBUG}
mv libsync.a libsyncmcstas.a
make -j40 libsync.a LOCK_VERSION=HYBRIDV2 ADD_PADDING=0 DEBUG=${DEBUG}
mv libsync.a libsyncbhl.a

cd ext/index-benchmarks
mkdir -p build && cd build
sudo apt-get install -y libtbb-dev libgoogle-glog-dev
cmake -DCMAKE_BUILD_TYPE=${BUILDMODE} ..
make -j40
