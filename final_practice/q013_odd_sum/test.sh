#!/usr/bin/env bash
# Q013 checks: 1 + 3 + ... + (2N-1) by a loop, N*N, and the equality flag.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on odd_sum_data (N = 300)" "$(run_sim -b solution.txt odd_sum_data.byte)" 0
check "S = 90000" "$(word_at "$RUN/odd_sum_data_1.out" 4)" 90000
check "N*N = 90000" "$(word_at "$RUN/odd_sum_data_1.out" 8)" 90000
check "flag = 1" "$(word_at "$RUN/odd_sum_data_1.out" 12)" 1
check "runs on odd_sum_data2 (N = 0)" "$(run_sim -b solution.txt odd_sum_data2.byte)" 0
check "empty sum S = 0" "$(word_at "$RUN/odd_sum_data2_1.out" 4)" 0
check "N*N = 0" "$(word_at "$RUN/odd_sum_data2_1.out" 8)" 0
check "flag = 1" "$(word_at "$RUN/odd_sum_data2_1.out" 12)" 1
finish
