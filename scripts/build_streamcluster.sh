#! /bin/bash
set -e

cd ext/parsec-benchmark

if [ "$1" == "clean" ]; then
  ./bin/parsecmgmt -a fullclean -p streamcluster
  ./bin/parsecmgmt -a fulluninstall -p streamcluster
  shift
fi

./bin/parsecmgmt -a build -p streamcluster
