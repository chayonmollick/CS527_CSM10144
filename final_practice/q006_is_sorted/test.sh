#!/usr/bin/env bash
# Q006 checks: sorted (non-decreasing) test, 1 or 0.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on is_sorted_data (N = 8)" "$(run_sim -b solution.txt is_sorted_data.byte)" 0
check "sorted with equal neighbours -> 1" "$(word_at "$RUN/is_sorted_data_1.out" 36)" 1
check "runs on is_sorted_data2 (N = 7)" "$(run_sim -b solution.txt is_sorted_data2.byte)" 0
check "only the last pair out of order -> 0" "$(word_at "$RUN/is_sorted_data2_1.out" 32)" 0
finish
