#! /bin/bash
set -e

cd ext/parsec-benchmark

if [ "$1" == "clean" ]; then
  ./bin/parsecmgmt -a fullclean -p dedup
  ./bin/parsecmgmt -a fulluninstall -p dedup
  shift
fi

./bin/parsecmgmt -a build -p dedup

if [ ! -f "pkgs/kernels/dedup/run/FC-6-x86_64-disc1.iso" ]; then
  ./get-inputs -n
  ./bin/parsecmgmt -a run -p dedup -i native
fi
