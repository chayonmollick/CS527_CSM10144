#!/usr/bin/env bash
# Q097 checks: the 8 lanes of every vector add/sub/mul run as one `omp simd` loop; every vector
# result is identical to the unmodified lab6 (plain loop) with 1 and 4 threads; the
# threads-per-lane experiment gives the same results but is slower (times printed, not checked).
. "$(dirname "$0")/../lib.sh"
build_solution
CXX_USED=$(cd "$WORK/sim" && make -s -n -B main.o | awk 'NR == 1 { print $1 }')   # compiler the Makefile chose
cp -r "$LAB6" "$WORK/ref" && rm -f "$WORK/ref/sim" "$WORK/ref"/*.o      # the unmodified lab6, same compiler
(cd "$WORK/ref" && make CXX="$CXX_USED" > /dev/null 2>&1) || { echo "lab6 reference build failed"; exit 1; }
cp -r "$WORK/sim" "$WORK/lanes"                                         # threads per lane
(cd "$WORK/lanes" && make clean > /dev/null \
    && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DQ097_LANE_THREADS" > /dev/null 2>&1) \
    || { echo "threads-per-lane build failed"; exit 1; }

PROGS="vec_ops.txt vec_ops_data.byte fir.txt fir_data.byte array_add.txt array_add_data.byte"
PROGS="$PROGS vec_heavy.txt vec_ops_data.byte"
run_in() {      # run_in <name> <sim> <threads>: PROGS in a fresh folder $WORK/<name>; prints the time
    mkdir -p "$WORK/$1" && cp "$RUN"/*.txt "$RUN"/*.byte "$WORK/$1"/
    local TIMEFORMAT=%3R
    { time (cd "$WORK/$1" && OMP_NUM_THREADS=$3 "$2" -b $PROGS > run.txt 2>&1); } 2>&1
}
same() {        # same <name>: "same" if every .out file equals the unmodified lab6's
    local f n=0
    for f in $(cd "$WORK/ref" && ls *.out); do
        cmp -s "$WORK/ref/$f" "$WORK/$1/$f" || { echo "$f differs"; return; }
        n=$((n + 1))
    done
    echo "same ($n files)"
}
for b in ref sim lanes; do run_in warm_$b "$WORK/$b/sim" 1 > /dev/null; done   # first starts
t_ref=$(run_in ref "$WORK/ref/sim" 1)
t_s1=$(run_in s1 "$SIM" 1)
t_s4=$(run_in s4 "$SIM" 4)
t_l4=$(run_in l4 "$WORK/lanes/sim" 4)
check "simd, 1 thread: all 4 tasks' data identical to lab6" "$(same s1)" "same (4 files)"
check "simd, 4 threads: identical to lab6" "$(same s4)" "same (4 files)"
check "threads per lane, 4 threads: identical to lab6" "$(same l4)" "same (4 files)"
check "vec_ops: lane 0 of v1 + v2 = 1 + 10" "$(word_at "$WORK/s4/vec_ops_data_1.out" 64)" 11
check "vec_ops: lane 7 of v1 + 255 wraps around" "$(word_at "$WORK/s4/vec_ops_data_1.out" 188)" -2147483394
check "vec_ops: lane 1 of v1 * x1 = -2 * 7" "$(word_at "$WORK/s4/vec_ops_data_1.out" 324)" -14
check "vec_ops: v1 = v1 * v1 (dest = both operands), lane 1 = 4" "$(word_at "$WORK/s4/vec_ops_data_1.out" 388)" 4
check "fir out[15] = 744, array_add C[15] = 131" \
    "$(word_at "$WORK/s4/fir_data_2.out" 316) $(word_at "$WORK/s4/array_add_data_3.out" 192)" "744 131"
echo "  time  (machine-specific, not checked; 12000 of the 18000 instructions of vec_heavy are vector)"
echo "        lab6, plain lane loop       : $t_ref s"
echo "        omp simd, 1 thread          : $t_s1 s"
echo "        omp simd, 4 threads         : $t_s4 s   (simd is inside one thread)"
echo "        threads per lane, 4 threads : $t_l4 s   (a parallel region per vector instruction)"

# Did the compiler really vectorize the loop? GCC says so with -fopt-info-vec (other compilers: no report).
L=$(grep -n 'pragma omp simd' "$WORK/sim/processor.cpp" | cut -d: -f1)
cp -r "$WORK/sim" "$WORK/report"
rep=$(cd "$WORK/report" && rm -f processor.o \
    && make CXXFLAGS="-std=c++17 -O2 -fopt-info-vec-optimized" processor.o 2>&1 \
    | awk -F: -v l="$L" '$1 == "processor.cpp" && $2 > l && $2 <= l + 5 && /vectorized/')
echo "  info  vectorizer, simd loop (pragma at line $L): ${rep:-no report from this compiler}"

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
