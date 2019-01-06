
# UnTracer-AFL

This repository contains an implementation of our prototype coverage-guided tracing framework UnTracer (as presented in our paper *[Full-speed Fuzzing: Reducing Fuzzing Overhead through Coverage-guided Tracing](https://arxiv.org/abs/1812.11875)*) in the popular coverage-guided fuzzer [AFL](http://lcamtuf.coredump.cx/afl). Coverage-guided tracing employs two versions of the target binary -- (1) a forkserver-only `oracle` modified with basic block-level software interrupts for quickly identifying coverage-increasing testcases; and (2) a fully-instrumented `tracer` for tracing the coverage of all coverage-increasing testcases. 

In UnTracer, both the oracle and tracer utilize the AFL-inspired [forkserver execution model](http://lcamtuf.blogspot.com/2014/10/fuzzing-binaries-without-execve.html). For `oracle` instrumentation we require all target binaries be compiled with `untracer-cc` -- our "forkserver-only" modification of AFL's assembly-time instrumenter `afl-cc`. For `tracer` binary instrumentation we utilize [Dyninst](http://www.dyninst.org/) with much of our code based off of [AFL-Dyninst](https://github.com/vanhauser-thc/afl-dyninst). We plan to incorporate a purely "black-box" (source-unavailable) instrumentation approach in the near future. Our current implementation of UnTracer supports **basic block coverage**. 

|             |                |
|-------------|----------------|
|**AUTHOR:**  | Stefan Nagy  |
|**EMAIL:**   | snagy2@vt.edu |
|**LICENSE:** | [MIT License](LICENSE) |
|**DISCLAIMER:**   | This software is strictly a research prototype. |


## INSTALLATION
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

## USAGE
First, compile all target binaries in "forkserver-only" mode using `untracer-clang` or `untracer-gcc`. Note that only **non-position-independent** target binaries are supported, so compile all target binaries with CFLAG `-no-pie` (unnecessary for Clang).

Then, run as follows:
```
untracer-afl -i [/path/to/seed/dir] -o [/path/to/out/dir] [optional_args] -- [/path/to/target] [target_args]
```

### Status Screen
<p align="center">
<img src="http://people.cs.vt.edu/snagy2/img/untracer-afl.png" width="700">
</p>

* `calib execs` and `trim execs` - Number of testcase calibration and trimming executions, respectively. Tracing is done for both.
* `block coverage` - Percentage of total blocks found (left) and the number of total blocks (right).
* `traced / queued` - Ratio of traced versus queued testcases. This ratio should (ideally) be 1:1 except for when trace timeouts occur.
* `trace tmouts (discarded)` - Number of testcases which timed out during tracing. Like AFL, we do not queue these.
* `no new bits (discarded)` - Number of testcases which were marked coverage-increasing by the oracle but did not actually increase coverage. This should (ideally) be 0.
