#! /bin/bash

BASE_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )/../../

${BASE_DIR}/scripts/chart_scheduling.py -t 5 -c 0 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/c0_t5.png
${BASE_DIR}/scripts/chart_scheduling.py -t 1 -c 0 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/c0_t1.png
${BASE_DIR}/scripts/chart_scheduling.py -t 5 -c 1000 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/c1000_t5.png
${BASE_DIR}/scripts/chart_scheduling.py -t 1 -c 1000 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/c1000_t1.png
${BASE_DIR}/scripts/chart_scheduling.py -t 5 -c 1000000 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/c1000000_t5.png
${BASE_DIR}/scripts/chart_scheduling.py -t 1 -c 1000000 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/c1000000_t1.png
