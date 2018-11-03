# Create evaluation binaries

Before running the evaluation scripts in this folder, you need to build the required target binaries:
* QEMU: Completely uninstrumented binary
* AFL-gcc: Binary with forkserver and basic block callbacks baked-in at compile time
* Baseline: Binary instrumented with init forkserver
* AFL-Dyninst: Binary instrumented with init forkserver and basic block callbacks
 
#### Download and build the uninstrumented target binary:

```
wget http://www.ijg.org/files/jpegsrc.v9a.tar.gz
tar -xzf jpegsrc.v9a.tar.gz
rm jpegsrc.v9a.tar.gz
cd jpeg-9a
./configure --disable-shared
# Append -no-pie to cflags in Makefile
make
cp djpeg /path/to/afl/cluster_eval/.
```

#### Download and build AFL: https://github.com/FoRTE-Research/afl
* Follow the provided build instructions for AFL and Qemu

```
export PATH=$PATH:/path/to/afl
export AFL_PATH=/path/to/afl
```

#### Create the AFL-gcc instrumented binary:

```
cd /path/to/jpeg-9a
./configure CC="afl-gcc" CXX="afl-g++" --disable-shared
make
cp djpeg /path/to/afl/cluster_eval/djpegWB
```

#### Download and build Dyninst: https://github.com/FoRTE-Research/UnTracer-Fuzzing
* Follow the provided build instructions

#### Download and build the init-only forkserver instrumentor: https://github.com/FoRTE-Research/forkserver-baseline
* update `DYN_ROOT` in `Makefile` to the path where you built Dyninst

```
export LD_LIBRARY_PATH=/path/to/dyninst_install/lib
export DYNINSTAPI_RT_LIB=/path/to/dyninst_install/lib/libdyninstAPI_RT.so
cp /path/to/afl/cluster_eval/djpeg .
make
cp djpegInst /path/to/afl/cluster_eval/djpegBaseline
```

#### Download, build, and instrument with AFL-Dyninst: https://github.com/FoRTE-Research/afl-dyninst
* update `DYN_ROOT` in `Makefile` to the path where you built Dyninst

```
export LD_LIBRARY_PATH=/path/to/dyninst_install/lib
export DYNINSTAPI_RT_LIB=/path/to/dyninst_install/lib/libdyninstAPI_RT.so
cp /path/to/afl/cluster_eval/djpeg .
make
cp djpegInst /path/to/afl/cluster_eval/djpegInst
```

# Prepare evaluation scripts
  
Edit the paths in `youEdit.sh` so that they match where things are on your system.
  
# Run single process test
  
You can run any PBS script as if it were a regular shell script, e.g.,
  
```
sh fuzzConBaseline.pbs
```
  
# Run cluster test
  
```
sh submitJobs.sh
```
  
# Result processing
  
`getTimes.py` reports the total time taken by all of the results in each passed file.
   
`filterResults.py` outputs a trimmed mean trace given the results in the passed files.
  
