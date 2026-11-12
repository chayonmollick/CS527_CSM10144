#!/usr/bin/env bash
# Q007 checks: integer average and the count of elements above it.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on above_average_data (N = 10)" "$(run_sim -b solution.txt above_average_data.byte)" 0
check "average 150 / 10 = 15" "$(word_at "$RUN/above_average_data_1.out" 44)" 15
check "3 elements > 15 (15 itself not counted)" "$(word_at "$RUN/above_average_data_1.out" 48)" 3
check "runs on above_average_data2 (negative sum)" "$(run_sim -b solution.txt above_average_data2.byte)" 0
check "average -27 / 5 = -5 (towards zero)" "$(word_at "$RUN/above_average_data2_1.out" 24)" -5
check "2 elements > -5" "$(word_at "$RUN/above_average_data2_1.out" 28)" 2
finish
