#! /bin/bash
set -e

cd ext/parsec-benchmark

if [ "$1" == "clean" ]; then
  ./bin/parsecmgmt -a fullclean -p splash2x.raytrace
  ./bin/parsecmgmt -a fulluninstall -p splash2x.raytrace
  shift
fi

./bin/parsecmgmt -a build -p splash2x.raytrace
./bin/parsecmgmt -a run -p splash2x.raytrace -i simsmall
