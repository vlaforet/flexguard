#! /bin/bash

echo "1000"
./scheduling_bench.sh -n 55 -c 1000
python3 chart.py
mv out.png out/1000.png

echo "0"
./scheduling_bench.sh -n 1 -c 0
python3 chart.py
mv out.png out/0.png

echo "100"
./scheduling_bench.sh -n 55 -c 100
python3 chart.py
mv out.png out/100.png

echo "10000"
./scheduling_bench.sh -n 55 -c 10000
python3 chart.py
mv out.png out/10000.png

echo "10000"
./scheduling_bench.sh -n 55 -c 100000
python3 chart.py
mv out.png out/100000.png

echo "1000000"
./scheduling_bench.sh -n 55 -c 1000000
python3 chart.py
mv out.png out/1000000.png
