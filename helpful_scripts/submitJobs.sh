#!/bin/bash

for i in `seq 1 24`
do
    qsub -l nodes=1:ppn=12 -l walltime=200:00:00 -d `pwd` fuzzConDyninst.pbs
#    qsub -l nodes=1:ppn=12 -l walltime=200:00:00 -d `pwd` fuzzConQemu.pbs
#    qsub -l nodes=1:ppn=12 -l walltime=200:00:00 -d `pwd` fuzzConWB.pbs
#    qsub -l nodes=1:ppn=12 -l walltime=200:00:00 -d `pwd` fuzzConBaseline.pbs
done
