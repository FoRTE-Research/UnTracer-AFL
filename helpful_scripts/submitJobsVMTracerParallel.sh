#!/bin/bash

home=`pwd`

for trial in `seq 1 11`
do
    export PBS_JOBID=$trial
    
    for day in `seq 1 1`
    do
	export FSF_DAY=$day
	export FSF_INPUTS_PATH=/media/sf_bigdata/day${FSF_DAY}
    
	for bench in bsdtar cert-basic cjson djpeg pdftohtml readelf sfconvert tcpdump
	do
	
	    cat ${bench}.sh > benchmark.sh
	    cat ${bench}.sh > ../../UnTracer/benchmark.sh

	    sh fuzzConBaseline.pbs
	    sh fuzzConDyninst.pbs
	    sh fuzzConWB.pbs
	    sh fuzzConQemu.pbs
	    cd ../../UnTracer
	    sh fuzzConUntracer.pbs
	    cd $home
	done
    done
done
