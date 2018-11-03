. ./benchmark.sh
export FSF_NUM_INPUTS=999999999
if test -z $FSF_INPUTS_PATH
then
    export FSF_INPUTS_PATH=/media/sf_bigdata/$FSF_BENCH
else
    export FSF_INPUTS_PATH=$FSF_INPUTS_PATH/$FSF_BENCH
fi
export FSF_RESULT_DIR=/media/sf_Desktop
export FSF_EVAL_DIR=/home/mdhicks2/Desktop/afl/cluster_eval
export FSF_AFL_DYNINST=/home/mdhicks2/Desktop/afl-dyninst
export FSF_BASELINE_DYNINST=/home/mdhicks2/Desktop/UnTracer
export FSF_DYNINST_INSTALL=/home/mdhicks2/Desktop/dynInstall
export FSF_DUMP=$FSF_INPUTS_PATH/_INPUT_DUMP
export FSF_SIZES=$FSF_INPUTS_PATH/_INPUT_SIZES
export LD_LIBRARY_PATH=$FSF_DYNINST_INSTALL/lib:$FSF_BASELINE_DYNINST:$FSF_AFL_DYNINST
export DYNINSTAPI_RT_LIB=$FSF_DYNINST_INSTALL/lib/libdyninstAPI_RT.so

echo PBS default server is $PBS_DEFAULT

cd $FSF_EVAL_DIR

echo Running on host `hostname`
echo Start time is `date`
echo Current directory is `pwd`

export AFL_SKIP_BIN_CHECK=1
echo core >/proc/sys/kernel/core_pattern
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
