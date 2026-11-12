#!/usr/bin/env bash
# Q051 checks: one warning per x register read before the task wrote it, with the source line.
. "$(dirname "$0")/../lib.sh"
build_solution
check "uninit and sum_array run together" "$(run_sim -b uninit.txt uninit_data.byte sum_array.txt sum_array_data.byte)" 0
check "3 warnings in all" "$(grep -c '^\[warn\]' "$RUN/run.txt")" 3
check "x5 at line 3 (line 4 reads it again: no second warning)" "$(grep -c '^\[warn\] pid 1 reads x5 before writing it, at line 3: x2 = x1 + x5 ' "$RUN/run.txt")" 1
check "x7 only once, although the loop reads it 3 times" "$(grep -c '^\[warn\] pid 1 reads x7 before writing it, at line 6: x7 = x7 + 1 ' "$RUN/run.txt")" 1
check "x9, the address register of a store, at line 10" "$(grep -c '^\[warn\] pid 1 reads x9 before writing it, at line 10: \[x9\] = x2 ' "$RUN/run.txt")" 1
check "sum_array writes every register before reading it" "$(grep -c '^\[warn\] pid 2' "$RUN/run.txt")" 0
check "a warning does not stop the task: [0] = 30, [4] = 3" "$(word_at "$RUN/uninit_data_1.out" 0),$(word_at "$RUN/uninit_data_1.out" 4)" "30,3"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
finish
