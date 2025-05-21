#! /bin/bash
set -e

cd ext/parsec-benchmark

if [ "$1" == "clean" ]; then
  ./bin/parsecmgmt -a fullclean -p raytrace
  ./bin/parsecmgmt -a fulluninstall -p raytrace
  shift
fi

./bin/parsecmgmt -a build -p raytrace
