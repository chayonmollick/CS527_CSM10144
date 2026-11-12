#!/usr/bin/env bash
# Q016 checks: mode of values 0..15 (ties -> smallest) and its count.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on mode_data (N = 12)" "$(run_sim -b solution.txt mode_data.byte)" 0
check "tie between 3 and 7 -> mode 3" "$(word_at "$RUN/mode_data_1.out" 52)" 3
check "count = 3" "$(word_at "$RUN/mode_data_1.out" 56)" 3
check "runs on mode_data2 (N = 9)" "$(run_sim -b solution.txt mode_data2.byte)" 0
check "mode = 15 (the top counter)" "$(word_at "$RUN/mode_data2_1.out" 40)" 15
check "count = 4" "$(word_at "$RUN/mode_data2_1.out" 44)" 4
finish
