#! /bin/bash
set -e

sudo apt update
sudo apt install -y libx11-dev xorg-dev

cd ext/parsec-benchmark

if [ "$1" == "clean" ]; then
  ./bin/parsecmgmt -a fullclean -p raytrace
  ./bin/parsecmgmt -a fulluninstall -p raytrace
  shift
fi

./bin/parsecmgmt -a build -p raytrace
./bin/parsecmgmt -a run -p raytrace -i simsmall
