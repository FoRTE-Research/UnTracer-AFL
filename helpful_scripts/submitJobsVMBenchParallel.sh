#!/bin/bash

bench=bsdtar

cat ${bench}.sh > benchmark.sh
cat ${bench}.sh > ../../UnTracer/benchmark.sh

home=`pwd`

for trial in `seq 1 11`
do
    export PBS_JOBID=$trial
    
    for day in `seq 1 5`
    do
	export FSF_DAY=$day
	export FSF_INPUTS_PATH=/media/sf_bigdata_grade10/day${FSF_DAY}

	sh fuzzConBaseline.pbs
	sh fuzzConDyninst.pbs
	sh fuzzConWB.pbs
	sh fuzzConQemu.pbs
	cd ../../UnTracer
	sh fuzzConUntracer.pbs
	cd $home
    done
done
