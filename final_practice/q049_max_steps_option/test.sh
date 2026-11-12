#!/usr/bin/env bash
# Q049 checks: -m N sets the instruction limit (MAX_STEPS) at run time.
. "$(dirname "$0")/../lib.sh"
build_solution
check "-m 100 stops the endless loop (exit code 1)" "$(run_sim -b -m 100 forever.txt forever_data.byte)" 1
check "the error shows the new limit" "$(grep -c '100 instructions executed without halting' "$RUN/run.txt")" 1
check "stopped after exactly 100 instructions" "$(grep -c 'STOPPED WITH AN ERROR on processor 0 after 100 instructions' "$RUN/run.txt")" 1
check "1 + 33 x 3 = 100: the counter reached 33" "$(word_at "$RUN/forever_data_1.out" 0)" 33
check "-m 71 is exactly enough for sum_array (71 instructions)" "$(run_sim -b -m 71 sum_array.txt sum_array_data.byte)" 0
check "-m 70 stops it just before its halt" "$(run_sim -b -m 70 sum_array.txt sum_array_data.byte)" 1
check "-m 0 is rejected with the usage text" "$(run_sim -b -m 0 sum_array.txt sum_array_data.byte),$(grep -c '^usage:' "$RUN/run.txt")" "1,1"
check "-t and -m together: exactly 5 instructions traced" "$(run_sim -b -t -m 5 forever.txt forever_data.byte),$(grep -c '^\[pid 1 p0\]' "$RUN/run.txt")" "1,5"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "without -m the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
