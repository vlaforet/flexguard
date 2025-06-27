#! /bin/bash
set -e

cd ext/parsec-benchmark

if [ "$1" == "clean" ]; then
  ./bin/parsecmgmt -a fullclean -p splash2x.volrend
  ./bin/parsecmgmt -a fulluninstall -p splash2x.volrend
  shift
fi

./bin/parsecmgmt -a build -p splash2x.volrend
./bin/parsecmgmt -a run -p splash2x.volrend -i native
