#!/usr/bin/env bash
# Q094 checks: of the waiting tasks, the one with the shortest program gets the next free processor.
. "$(dirname "$0")/../lib.sh"
build_solution

# spin jobs of 2, 4, 6, 6 rounds hold the processors; long (10), short (3), medium (6), short (3) wait.
check "8 tasks run without error" "$(run_sim -b spin.txt r2.byte spin.txt r4.byte spin.txt r6.byte spin.txt r6.byte \
      long.txt sjf_data.byte short.txt sjf_data.byte medium.txt sjf_data.byte short.txt sjf_data.byte)" 0
check "pids 5-8 wait for a processor" "$(grep -c 'is waiting for a free processor' "$RUN/run.txt")" 4
check "admitted shortest first (short 6, short 8 - equal length keeps arrival order - medium 7, long 5)" \
      "$(awk '/loaded on processor/ && $3 > 4 { printf "%s%s", s, $3; s = " " } END { print "" }' "$RUN/run.txt")" "6 8 7 5"
check "results: long 8, short 1, medium 4, short 1" \
      "$(for p in 5 6 7 8; do printf '%s ' "$(word_at "$RUN/sjf_data_$p.out" 0)"; done)" "8 1 4 1 "

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
