# UnTracer-AFL
This repository contains an implementation of our prototype coverage-guided tracing framework **UnTracer** in the popular coverage-guided fuzzer [AFL](http://lcamtuf.coredump.cx/afl). Coverage-guided tracing employs two versions of the target binary: (1) a forkserver-only `oracle` binary modified with basic block-level software interrupts on unseen basic blocks for quickly identifying coverage-increasing testcases and (2) a fully-instrumented `tracer` binary for tracing the coverage of all coverage-increasing testcases. 

In UnTracer, both the oracle and tracer binaries use the AFL-inspired [forkserver execution model](http://lcamtuf.blogspot.com/2014/10/fuzzing-binaries-without-execve.html). For `oracle` instrumentation we require all target binaries be compiled with `untracer-cc` -- our "forkserver-only" modification of AFL's assembly-time instrumenter `afl-cc`. For `tracer` binary instrumentation we utilize [Dyninst](http://www.dyninst.org/) with much of our code based-off [AFL-Dyninst](https://github.com/vanhauser-thc/afl-dyninst). We plan to incorporate a purely "black-box" (source-unavailable) instrumentation approach in the near future. Our current implementation of UnTracer supports **basic block coverage**. 

<table>
  <tr>
    <td align=center colspan="2"><div><b>Presented in our paper</b> <a href="https://arxiv.org/abs/1812.11875"><i>Full-speed Fuzzing: Reducing Fuzzing Overhead through Coverage-guided Tracing</i></a><br>(to appear in the 2019 IEEE Symposium on Security and Privacy).</td>
  </tr>
  <tr>
    <td><b>Citing this repository:</b></td>
    <td>
      <code class="rich-diff-level-one">@inproceedings{nagy:fullspeedfuzzing,</code><br>
      <code class="rich-diff-level-one">title = {Full-speed Fuzzing: Reducing Fuzzing Overhead through Coverage-guided Tracing},</code><br>
      <code class="rich-diff-level-one">author = {Stefan Nagy and Matthew Hicks},</code><br>
      <code class="rich-diff-level-one">booktitle = {{IEEE} Symposium on Security and Privacy (Oakland)},</code><br>
      <code class="rich-diff-level-one">year = {2019},}</code>
    </td>
  </tr>
  <tr>
    <td><b>Developers:</b></td>
    <td>Stefan Nagy (<a href="mailto:snagy2@vt.edu">snagy2@vt.edu</a>) and Matthew Hicks (<a href="mailto:mdhicks2@vt.edu">mdhicks2@vt.edu</a>)</td>
  </tr>
  <tr>
    <td><b>License:</b></td>
    <td><a href="/FoRTE-Research/UnTracer-AFL/blob/master/LICENSE">MIT License</a></td>
  </tr>
  <tr>
    <td><b>Disclaimer:</b></td>
    <td><i>This software is strictly a research prototype.</i></td>
  </tr>
</table>


## INSTALLATION
#### 1. Download and build Dyninst (we used v9.3.2)
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

#### 2. Download UnTracer-AFL (this repo)
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
First, compile all target binaries using "forkserver-only" instrumentation. [As with AFL](https://github.com/mcarpenter/afl), you will need to manually set the C compiler (`untracer-clang` or `untracer-gcc`) and/or C++ compiler (`untracer-clang++` or `untracer-g++`). Note that only **non-position-independent** target binaries are supported, so compile all target binaries with CFLAG `-no-pie` (unnecessary for Clang). For example:

**NOTE:** We provide a set of **fuzzing-ready benchmarks** available here: [https://github.com/FoRTE-Research/FoRTE-FuzzBench](https://github.com/FoRTE-Research/FoRTE-FuzzBench).

```
$ CC=/path/to/afl/untracer-clang ./configure --disable-shared
$ CXX=/path/to/afl/untracer-clang++.
$ make clean all
Instrumenting in forkserver-only mode...
```

Then, run `untracer-afl` as follows:

```
untracer-afl -i [/path/to/seed/dir] -o [/path/to/out/dir] [optional_args] -- [/path/to/target] [target_args]
```

### Status Screen
<p align="center">
<img src="http://people.cs.vt.edu/snagy2/img/untracer-afl.png" width="700">
</p>

* `calib execs` and `trim execs` - Number of testcase calibration and trimming executions, respectively. Tracing is done for both.
* `block coverage` - Percentage of total blocks found (left) and the number of total blocks (right).
* `traced / queued` - Ratio of traced versus queued testcases. This ratio should (ideally) be 1:1 but will increase as trace timeouts occur.
* `trace tmouts (discarded)` - Number of testcases which timed out during tracing. Like AFL, we do not queue these.
* `no new bits (discarded)` - Number of testcases which were marked coverage-increasing by the oracle but did not actually increase coverage. This should (ideally) be 0.

#
<p align=center> <a href="https://www.cs.vt.edu"><img border="0" src="http://people.cs.vt.edu/snagy2/img/vt_inline_computer_science.png" width="60%" height="60%">
</a> </p>
