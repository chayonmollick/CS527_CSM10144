#!/usr/bin/env bash
# Q095 checks: option -k K keeps at most K tasks in memory at once, even when processors are free.
. "$(dirname "$0")/../lib.sh"
log_pids() { awk '{ printf "%s%s", s, $3; s = " " } END { print "" }' "$RUN/log.txt"; }
T=(print3.txt print3_data.byte)        # 15 instructions = 2 time slices; prints 1, 2, 3
build_solution

rm -f "${RUN:?}/log.txt"
check "-k 2 with 4 tasks runs without error" "$(run_sim -b -k 2 "${T[@]}" "${T[@]}" "${T[@]}" "${T[@]}")" 0
check "only 2 loaded at first, pids 3 and 4 wait" "$(grep -c 'is waiting for a free processor' "$RUN/run.txt")" 2
check "processors 2 and 3 are never used" "$(grep -c 'loaded on processor [23]' "$RUN/run.txt")" 0
check "pids 3, 4 start when 1, 2 have finished (pids in log.txt)" "$(log_pids)" "1 1 2 2 1 2 3 3 4 4 3 4"
check "all four counted to 3" \
      "$(for p in 1 2 3 4; do printf '%s ' "$(word_at "$RUN/print3_data_$p.out" 0)"; done)" "3 3 3 3 "

rm -f "${RUN:?}/log.txt"
run_sim -b -k 1 "${T[@]}" "${T[@]}" "${T[@]}" > /dev/null
check "-k 1: the tasks run one after another" "$(log_pids)" "1 1 1 2 2 2 3 3 3"

rm -f "${RUN:?}/log.txt"
run_sim -b "${T[@]}" "${T[@]}" "${T[@]}" "${T[@]}" > /dev/null
check "without -k all 4 run at once, as in lab6" "$(log_pids)" "1 1 2 2 3 3 4 4 1 2 3 4"

check "-k 0 is refused" "$(run_sim -b -k 0 "${T[@]}")" 1
check "-k 5 is refused (NP = 4)" "$(run_sim -b -k 5 "${T[@]}")" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
