#!/usr/bin/env bash
# Q001 checks: second largest distinct value and the index of its first occurrence.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on second_largest_data (N = 12)" "$(run_sim -b solution.txt second_largest_data.byte)" 0
check "second largest = 41 (45 is the max, twice)" "$(word_at "$RUN/second_largest_data_1.out" 52)" 41
check "index of its first occurrence = 3" "$(word_at "$RUN/second_largest_data_1.out" 56)" 3
check "runs on second_largest_data2 (all -7)" "$(run_sim -b solution.txt second_largest_data2.byte)" 0
check "no second value: value 0" "$(word_at "$RUN/second_largest_data2_1.out" 24)" 0
check "no second value: index -1" "$(word_at "$RUN/second_largest_data2_1.out" 28)" -1
finish
