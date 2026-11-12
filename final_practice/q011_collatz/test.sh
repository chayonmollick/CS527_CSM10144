#!/usr/bin/env bash
# Q011 checks: Collatz steps from n to 1 and the largest value reached.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on collatz_data (n = 27)" "$(run_sim -b solution.txt collatz_data.byte)" 0
check "27 needs 111 steps" "$(word_at "$RUN/collatz_data_1.out" 4)" 111
check "largest value reached = 9232" "$(word_at "$RUN/collatz_data_1.out" 8)" 9232
check "runs on collatz_data2 (n = 1)" "$(run_sim -b solution.txt collatz_data2.byte)" 0
check "1 needs 0 steps" "$(word_at "$RUN/collatz_data2_1.out" 4)" 0
check "largest value = 1" "$(word_at "$RUN/collatz_data2_1.out" 8)" 1
finish
