#! /bin/bash

cd ext/parsec-benchmark
./get-inputs -n
./bin/parsecmgmt -a build -p dedup
./bin/parsecmgmt -a run -p dedup -i native