#!/bin/bash
THE_LOCKS="HCLH TTAS ARRAY MCS TICKET HTICKET MUTEX SPINLOCK CLH"
make="make"

rm correctness.out

for prefix in ${THE_LOCKS}
do
cd ..; LOCK_VERSION=-DUSE_${prefix}_LOCKS PRIMITIVE=-DTEST_CAS OPTIMIZE=${optimize} PLATFORM=${platform_def} ${make} clean all; cd scripts;
echo ${prefix} >> correctness.out
${prog_prefix}test_correctness -n ${num_cores} -d 1000 >> correctness.out
done

