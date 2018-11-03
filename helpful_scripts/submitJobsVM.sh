#!/bin/bash

for i in `seq 1 11`
do
    export PBS_JOBID=$i
    sh fuzzConBaseline.pbs
    sh fuzzConDyninst.pbs
    sh fuzzConWB.pbs
    sh fuzzConQemu.pbs
done
