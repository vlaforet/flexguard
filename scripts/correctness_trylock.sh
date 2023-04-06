#!/bin/bash
THE_LOCKS="TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK"
make="make"

rm correctness_trylock.out

for prefix in ${THE_LOCKS}
do
cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;
echo ${prefix} >> correctness_trylock.out
${prog_prefix}test_trylock -n ${num_cores} -d 1000 >> correctness_trylock.out
done

