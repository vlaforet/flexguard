#! /bin/bash

BASE_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )/../../

${BASE_DIR}/scripts/chart_scheduling_contention.py -l futex mcs hybridlock -t 5 -n 30 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/contention_t5_n30.png
${BASE_DIR}/scripts/chart_scheduling_contention.py -l futex mcs hybridlock -t 1 -n 30 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/contention_t1_n30.png
${BASE_DIR}/scripts/chart_scheduling_contention.py -l futex mcs hybridlock -t 5 -n 40 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/contention_t5_n40.png
${BASE_DIR}/scripts/chart_scheduling_contention.py -l futex mcs hybridlock -t 1 -n 40 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/contention_t1_n40.png
${BASE_DIR}/scripts/chart_scheduling_contention.py -l futex mcs hybridlock -t 5 -n 50 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/contention_t5_n50.png
${BASE_DIR}/scripts/chart_scheduling_contention.py -l futex mcs hybridlock -t 1 -n 50 -i ${BASE_DIR}/results/ -o ${BASE_DIR}/results/contention_t1_n50.png
