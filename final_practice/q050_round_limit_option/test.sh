#!/usr/bin/env bash
# Q050 checks: -r R stops every task that has not finished after R scheduler rounds.
. "$(dirname "$0")/../lib.sh"
build_solution
check "-r 10: endless loop + sum_array (exit code 1: a task was stopped)" "$(run_sim -b -r 10 forever.txt forever_data.byte sum_array.txt sum_array_data.byte)" 1
check "the loop is stopped after its 10th round" "$(grep -c 'pid 1 has not finished after 10 scheduler rounds: stopped' "$RUN/run.txt")" 1
check "10 rounds x 10 instructions = 100 instructions" "$(grep -c 'pid 1 (forever) STOPPED WITH AN ERROR on processor 0 after 100 instructions' "$RUN/run.txt")" 1
check "1 + 33 x 3 = 100: the counter reached 33" "$(word_at "$RUN/forever_data_1.out" 0)" 33
check "sum_array (8 rounds) finishes normally beside it" "$(grep -c 'pid 2 (sum_array) finished' "$RUN/run.txt"),$(word_at "$RUN/sum_array_data_2.out" 36)" "1,45"
check "-r 8: enough for sum_array (71 = 7 x 10 + 1)" "$(run_sim -b -r 8 sum_array.txt sum_array_data.byte)" 0
check "-r 7: sum_array stopped after 70 instructions" "$(run_sim -b -r 7 sum_array.txt sum_array_data.byte),$(grep -c 'after 70 instructions' "$RUN/run.txt")" "1,1"
check "-r 0 is rejected with the usage text" "$(run_sim -b -r 0 sum_array.txt sum_array_data.byte),$(grep -c '^usage:' "$RUN/run.txt")" "1,1"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "without -r the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
