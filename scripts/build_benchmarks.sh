#! /bin/bash
set -e

./scripts/build_leveldb-1.20.sh $*
./scripts/build_index-benchmarks.sh $*
./scripts/build_raytrace.sh $*
./scripts/build_streamcluster.sh $*
./scripts/build_volrend.sh $*
./scripts/build_dedup.sh $*