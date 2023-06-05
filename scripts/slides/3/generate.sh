#! /bin/bash

BASE_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )/../../../

mkdir -p ${BASE_DIR}/results/slide3

${BASE_DIR}/scripts/chart_scheduling.py -t 1 -c 1000 -l Futex -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/slide3/1.png
${BASE_DIR}/scripts/chart_scheduling.py --increasing-only -t 1 -c 1000 -l Futex -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/slide3/2.png