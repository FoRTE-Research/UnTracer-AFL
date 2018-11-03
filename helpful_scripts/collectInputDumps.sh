rootDir=/home/mdhicks2/Desktop/fuzzing-benchmarks
baseTargetDir=/media/sf_bigdata
minToCollect=60
timeout=500

export AFL_SKIP_BIN_CHECK=1
echo core >/proc/sys/kernel/core_pattern
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

for day in `seq 1 5`
do
    targetDir=${baseTargetDir}/day${day}
    mkdir -p ${targetDir}
    ./afl-fuzz-saveinputs -i ${rootDir}/binutils/seed_dir -o ${targetDir}/readelf -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/binutils/binutils-2.30/binutils/readelf -a @@
    #./afl-fuzz-saveinputs -i ${rootDir}/libjpeg/seed_dir/ -o ${targetDir}/djpeg -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/libjpeg/jpeg-9c/djpeg @@
    #./afl-fuzz-saveinputs -i ${rootDir}/libarchive/seed_dir/ -o ${targetDir}/bsdtar -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/libarchive/libarchive-3.3.2/bsdtar -O -xf @@
    #./afl-fuzz-saveinputs -i ${rootDir}/tcpdump/seed_dir/ -o ${targetDir}/tcpdump -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/tcpdump/tcpdump-4.9.2/tcpdump -nr @@
    #./afl-fuzz-saveinputs -i ${rootDir}/audiofile/seed_dir/ -o ${targetDir}/sfconvert -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/audiofile/audiofile-0.2.7/sfcommands/sfconvert @@ out.mp3 format aiff
    #./afl-fuzz-saveinputs -i ${rootDir}/poppler/seed_dir/ -x ${rootDir}/poppler/pdf.dict -o ${targetDir}/pdftohtml -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/poppler/poppler-0.22.5/utils/pdftohtml @@
    #./afl-fuzz-saveinputs -i ${rootDir}/libksba/seed_dir/ -o ${targetDir}/cert-basic -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/libksba/libksba-1.3.5/tests/cert-basic @@
    #./afl-fuzz-saveinputs -i ${rootDir}/cjson/seed_dir/ -o ${targetDir}/cjson -x ${rootDir}/cjson/json.dict -t ${timeout} -e ${minToCollect} -Q -- ${rootDir}/cjson/cjson-1.7.7/fuzzing/cjson @@
done
