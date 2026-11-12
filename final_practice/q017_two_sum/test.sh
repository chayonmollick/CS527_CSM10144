#!/usr/bin/env bash
# Q017 checks: first pair i < j with A[i] + A[j] = T, or -1 -1.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on two_sum_data (N = 8, T = 10)" "$(run_sim -b solution.txt two_sum_data.byte)" 0
check "i = 2 (A[2] = -2)" "$(word_at "$RUN/two_sum_data_1.out" 40)" 2
check "j = 5 (A[5] = 12)" "$(word_at "$RUN/two_sum_data_1.out" 44)" 5
check "runs on two_sum_data2 (T = 100, no pair)" "$(run_sim -b solution.txt two_sum_data2.byte)" 0
check "no pair: i = -1" "$(word_at "$RUN/two_sum_data2_1.out" 28)" -1
check "no pair: j = -1" "$(word_at "$RUN/two_sum_data2_1.out" 32)" -1
finish
