#!/bin/bash

THREADS="1 5 10"
TESTS="fillseq fillseqsync fillrandsync fillrandom overwrite readrandom readseq fillrand100K fillseq100K readseqsmall readrandsmall"
ENTRIES=1000000
OUTPUT="results/kyoto.csv"

usage() {
    echo "$0 [-n threads]"
    echo "    -n threads    List of threads to test"
    echo "                  Default: $THREADS"
    echo "    -t test       List of tests"
    echo "                  Default: $TESTS"
    echo "    -e entries    Number of entries"
    echo "                  Default: $ENTRIES"
    echo "    -o output     Output file"
    echo "                  Default: $OUTPUT"
}

while getopts "hn:t:e:o:" OPTION; do
    case $OPTION in
    h)
        usage
        exit 1
        ;;
    n)
        THREADS="$OPTARG"
        ;;
    t)
        TESTS="$OPTARG"
        ;;
    e)
        ENTRIES="$OPTARG"
        ;;
    o)
        OUTPUT="$OPTARG"
        ;;
    ?)
        usage
        exit
        ;;
    esac
done

declare -A results
for n in $THREADS; do
    echo "threads=$n"
    res=$(ext/leveldb/build/db_bench_tree_db --threads=$n --num=$ENTRIES --benchmarks="$(echo $TESTS | tr ' ' ',')")
    res=$(echo "$res" | sed -n '/^------------------------------------------------/,$p' | tail -n +2)

    while IFS= read -r line; do
        test_name=$(echo "$line" | cut -d':' -f1 | xargs)
        micros=$(echo "$line" | grep -oP '[\d.]+(?= micros/op)')

        if [[ -n "$test_name" && -n "$micros" ]]; then
            key="$n,$test_name"
            count=1
            while [[ -n "${results["$key"]}" ]]; do
                count=$((count + 1))
                key="$n,${test_name}_$count"
            done

            results["$key"]="$micros"
        fi
    done <<<"$res"
done

declare -a test_names_array=()
for key in "${!results[@]}"; do
    IFS=',' read -r thread test_name <<<"$key"
    if [[ ! " ${test_names_array[@]} " =~ " $test_name " ]]; then
        test_names_array+=("$test_name")
    fi
done

header="threads"
for test_name in ${test_names_array[@]}; do
    header="$header,$test_name"
done
echo "$header" >"$OUTPUT"

for n in $THREADS; do
    line="$n"
    for test_name in ${test_names_array[@]}; do
        line="$line,${results["$n,$test_name"]}"
    done
    echo "$line" >>"$OUTPUT"
done
