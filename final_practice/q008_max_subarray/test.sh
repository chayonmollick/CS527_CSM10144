#!/usr/bin/env bash
# Q008 checks: Kadane's maximum subarray sum with its start and end index.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on max_subarray_data (N = 9)" "$(run_sim -b solution.txt max_subarray_data.byte)" 0
check "max sum = 6 (4 -1 2 1)" "$(word_at "$RUN/max_subarray_data_1.out" 40)" 6
check "start index = 3" "$(word_at "$RUN/max_subarray_data_1.out" 44)" 3
check "end index = 6" "$(word_at "$RUN/max_subarray_data_1.out" 48)" 6
check "runs on max_subarray_data2 (all negative)" "$(run_sim -b solution.txt max_subarray_data2.byte)" 0
check "max sum = -2 (the largest element)" "$(word_at "$RUN/max_subarray_data2_1.out" 24)" -2
check "start index = 3" "$(word_at "$RUN/max_subarray_data2_1.out" 28)" 3
check "end index = 3" "$(word_at "$RUN/max_subarray_data2_1.out" 32)" 3
finish
