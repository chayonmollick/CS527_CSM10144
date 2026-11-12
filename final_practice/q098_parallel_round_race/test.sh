#!/usr/bin/env bash
# Q098 checks: each round's slices run in parallel (threadprivate + critical) and every result
# is the same as with one thread; then the race demonstration: the same build without the
# threadprivate lines (reported, not asserted: a race does not show on every run).
. "$(dirname "$0")/../lib.sh"
build_solution
FOUR="long_loop.txt long_loop_data.byte long_loop.txt long_loop_data.byte"
FOUR="$FOUR $FOUR"                                      # 4 tasks, one per processor

results() {     # the 4 tasks' results, e.g. 6000,6000,6000,6000,
    local r="" pid
    for pid in 1 2 3 4; do
        if [ -f "$RUN/long_loop_data_$pid.out" ]; then r="$r$(word_at "$RUN/long_loop_data_$pid.out" 0),"
        else r="${r}none,"; fi
    done
    echo "$r"
}
seconds() { local TIMEFORMAT=%3R; { time run_sim "$@" > /dev/null; } 2>&1; }   # wall time of one run

run_sim -b $FOUR > /dev/null                           # warm-up (first start loads libgomp)
for t in 1 4; do
    rm -f "$RUN"/long_loop_data_*.out
    s=$(OMP_NUM_THREADS=$t seconds -b $FOUR)
    echo "  time  4 x long_loop, $t thread(s): $s s (machine-specific, not checked)"
    check "$t thread(s): all 4 tasks computed 6000" "$(results)" "6000,6000,6000,6000,"
    check "$t thread(s): every task ran 8006 instructions" "$(grep -c 'after 8006 instructions' "$RUN/run.txt")" 4
done
bad=0
for r in 1 2 3 4 5 6 7 8 9 10; do
    rm -f "$RUN"/long_loop_data_*.out
    OMP_NUM_THREADS=4 run_sim -b $FOUR > /dev/null
    [ "$(results)" = "6000,6000,6000,6000," ] || bad=$((bad + 1))
done
check "10 more runs with 4 threads: none wrong" "$bad" 0

# Mixed workload: vectors, print (critical(log_file)) and page faults (critical(frames)).
rm -f "$RUN/log.txt"
OMP_NUM_THREADS=4 run_sim -b fir.txt fir_data.byte array_add.txt array_add_data.byte \
    sum_array_print.txt sum_array_data.byte pages.txt pages_data.byte > /dev/null
check "4 threads: fir out[15] = 744" "$(word_at "$RUN/fir_data_1.out" 316)" 744
check "4 threads: array_add C[15] = 131" "$(word_at "$RUN/array_add_data_2.out" 192)" 131
check "4 threads: sum_array_print = 45" "$(word_at "$RUN/sum_array_data_3.out" 36)" 45
check "4 threads: pages (3 page faults) = 21" "$(word_at "$RUN/pages_data_4.out" 0)" 21
check "4 threads: log.txt has all 9 print lines" "$(wc -l < "$RUN/log.txt" | tr -d ' ')" 9
bad=0
for r in 1 2 3 4 5 6 7 8 9 10; do                       # 3 tasks page-faulting at the same time
    rm -f "$RUN"/pages_data_*.out
    OMP_NUM_THREADS=4 run_sim -b pages.txt pages_data.byte pages.txt pages_data.byte \
        pages.txt pages_data.byte > /dev/null
    for pid in 1 2 3; do [ "$(word_at "$RUN/pages_data_$pid.out" 0)" = 21 ] || bad=$((bad + 1)); done
done
check "10 runs x 3 tasks with simultaneous page faults all = 21" "$bad" 0

# Race demonstration: delete the two threadprivate lines and build again.
cp -r "$WORK/sim" "$WORK/race"
for f in processor.h processor.cpp; do
    grep -v 'omp threadprivate' "$WORK/race/$f" > "$WORK/race/$f.new" && mv "$WORK/race/$f.new" "$WORK/race/$f"
done
(cd "$WORK/race" && make clean > /dev/null && make > /dev/null 2>&1) || { echo "race build failed"; exit 1; }
GOOD="$SIM"; SIM="$WORK/race/sim"
rm -f "$RUN"/long_loop_data_*.out
OMP_NUM_THREADS=1 run_sim -b $FOUR > /dev/null
check "without threadprivate, 1 thread is still right (no second thread)" "$(results)" "6000,6000,6000,6000,"
wrong=0
for r in 1 2 3 4 5; do
    rm -f "$RUN"/long_loop_data_*.out
    OMP_NUM_THREADS=4 run_sim -b $FOUR > /dev/null
    [ "$(results)" = "6000,6000,6000,6000," ] || wrong=$((wrong + 1))
done
echo "  race  without threadprivate, 4 threads: $wrong of 5 runs wrong; last run: $(results)"
echo "        instruction counts: $(grep -o 'after [0-9]* instructions' "$RUN/run.txt" | awk '{printf "%s ", $2}')(8006 expected)"
SIM="$GOOD"

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
