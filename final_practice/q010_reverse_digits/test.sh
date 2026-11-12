#!/usr/bin/env bash
# Q010 checks: reversed digits (sign kept) and digit sum.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on reverse_digits_data (n = 12340)" "$(run_sim -b solution.txt reverse_digits_data.byte)" 0
check "reversed = 4321" "$(word_at "$RUN/reverse_digits_data_1.out" 4)" 4321
check "digit sum = 10" "$(word_at "$RUN/reverse_digits_data_1.out" 8)" 10
check "runs on reverse_digits_data2 (n = -507)" "$(run_sim -b solution.txt reverse_digits_data2.byte)" 0
check "reversed = -705" "$(word_at "$RUN/reverse_digits_data2_1.out" 4)" -705
check "digit sum = 12" "$(word_at "$RUN/reverse_digits_data2_1.out" 8)" 12
finish
