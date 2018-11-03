# UnTracer-AFL
This repository contains an implementation of our coverage-guided tracing framework [UnTracer](https://github.com/FoRTE-Research/Untracer) in the popular coverage-guided fuzzer [AFL](http://lcamtuf.coredump.cx/afl).

**DISCLAIMER:** This software is strictly a research prototype.

## Getting Started
#### 1. Build Dyninst
```
sudo apt-get install cmake m4 zlib1g-dev libboost-all-dev libiberty-dev
wget https://github.com/dyninst/dyninst/archive/v9.3.2.tar.gz
tar -xf v9.3.2.tar.gz dyninst-9.3.2/
mkdir dynBuildDir
cd dynBuildDir
cmake ../dyninst-9.3.2/ -DCMAKE_INSTALL_PREFIX=`pwd`
make
make install
```

#### 2. Download UnTracer-AFL
```
git clone https://github.com/FoRTE-Research/UnTracer-AFL
```

#### 3. Configure environment variables
```
export DYNINST_INSTALL=''
export UNTRACER_AFL_PATH=''

export DYNINSTAPI_RT_LIB=$DYNINST_INSTALL/lib/libdyninstAPI_RT.so
export LD_LIBRARY_PATH=$DYNINST_INSTALL/lib:$UNTRACER_AFL_PATH
export PATH=$PATH:$UNTRACER_AFL_PATH
```

#### 4. Build UnTracer-AFL
Update `DYN_ROOT` in `UnTracer-AFL/Makefile` to your Dyninst install directory. 
Then, run the following commands:
```
make clean && make all
```

## Running UnTracer-AFL
First, compile all binaries using [FoRTE-afl-cc's forkserver-only ("basline")](https://github.com/FoRTE-Research/afl#forte-afl-cc) mode. Note that only **non-position-independent** target binaries are supported, so compile all target binaries with CFLAG `-no-pie` (unnecessary for Clang).

Then, run as follows:
```
untracer-afl -i [/path/to/seed/dir] -o [/path/to/out/dir] -- [/path/to/target-BASELINE] [target_args]
```

## Running QSYM-UnTracer-AFL:
See [here](https://github.com/FoRTE-Research/qsym#run-qsym-untracer-afl-for-24-hrs).

## Checking accumulated coverage with Cov-Check:
We provide a utility `cov-check` to calculate some block coverage statistics given an input queue. Note that like UnTracer-AFL, this tool also requires the forkserver-only (baseline) instrumented version of the target binary.

Run as follows:
```
cov-check -q [/path/to/out/dir/queue] -o [/path/to/working/dir] -f [/path/to/log] -t [optional timeout] -- [/path/to/target-BASELINE] [target_args]
```

This tool can also be similarly used on AFL or (below) QSYM-UnTracer-AFL input queues:
```
cov-check -q [/path/to/out/dir/queue] -o [/path/to/working/dir/afl-slave] -f [/path/to/log] -t [optional timeout] -- [/path/to/target-BASELINE] [target_args]
```
