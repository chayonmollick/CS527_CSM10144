#!/usr/bin/env bash
# Q099 checks: with the round's slices in parallel, the three instruction counters (critical,
# atomic, per-processor sum) all equal the real total (4N + 5 per task) on every run; then the
# -k benchmark times each way on its own (timings printed, not checked).
. "$(dirname "$0")/../lib.sh"
build_solution
TASKS="count_loop.txt n1000.byte count_loop.txt n2000.byte count_loop.txt n3000.byte count_loop.txt n4000.byte"
EXPECTED=$(( (4 * 1000 + 5) + (4 * 2000 + 5) + (4 * 3000 + 5) + (4 * 4000 + 5) ))   # 40020

counters() {    # "critical,atomic,per-processor" from the report line
    awk '/instructions executed by all processors/ { gsub(",", ""); print $8 "," $10 "," $13 }' "$RUN/run.txt"
}
task_sum() {    # sum of the per-task counts the OS prints ("after N instructions")
    grep -o 'after [0-9]* instructions' "$RUN/run.txt" | awk '{ s += $2 } END { print s }'
}
seconds() { local TIMEFORMAT=%3R; { time run_sim "$@" > /dev/null; } 2>&1; }

run_sim -b $TASKS > /dev/null                          # warm-up (first start loads libgomp)
for t in 1 4; do
    s=$(OMP_NUM_THREADS=$t seconds -b $TASKS)
    echo "  time  4 tasks (40020 instructions), $t thread(s): $s s (machine-specific, not checked)"
    echo "        $(grep 'instructions executed by all' "$RUN/run.txt")"
    check "$t thread(s): critical, atomic and per-processor all = $EXPECTED" "$(counters)" "$EXPECTED,$EXPECTED,$EXPECTED"
    check "$t thread(s): the per-task counts add up to the same total" "$(task_sum)" "$EXPECTED"
    check "$t thread(s): results 3N = 3000 6000 9000 12000" \
        "$(word_at "$RUN/n1000_1.out" 4) $(word_at "$RUN/n2000_2.out" 4) $(word_at "$RUN/n3000_3.out" 4) $(word_at "$RUN/n4000_4.out" 4)" \
        "3000 6000 9000 12000"
done
bad=0
for r in 1 2 3 4 5 6 7 8 9 10; do
    OMP_NUM_THREADS=4 run_sim -b $TASKS > /dev/null
    [ "$(counters)" = "$EXPECTED,$EXPECTED,$EXPECTED" ] || bad=$((bad + 1))
done
check "10 more runs with 4 threads: no update ever lost" "$bad" 0

# The counting cost on its own: N = 1000000 increments per thread, each way.
for t in 1 4; do
    OMP_NUM_THREADS=$t run_sim -k 1000000 > /dev/null
    sed 's/^/        /' "$RUN/run.txt"
    total=$((1000000 * t))
    check "benchmark, $t thread(s): critical, atomic, per-processor all = $total" \
        "$(awk '$1 == "critical" || $1 == "atomic" || $1 == "per-processor" { printf "%s,", $2 }' "$RUN/run.txt")" \
        "$total,$total,$total,"
done
echo "  note  'no protection' with 4 threads is a data race: usually WRONG, not checked"

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
