#!/usr/bin/env bash
# Q005 checks: the missing number of 1..N from N(N+1)/2 minus the numbers present.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on missing_number_data (N = 25)" "$(run_sim -b solution.txt missing_number_data.byte)" 0
check "missing number = 13 at 4N = 100" "$(word_at "$RUN/missing_number_data_1.out" 100)" 13
check "runs on missing_number_data2 (N = 1, empty list)" "$(run_sim -b solution.txt missing_number_data2.byte)" 0
check "missing number = 1 at 4N = 4" "$(word_at "$RUN/missing_number_data2_1.out" 4)" 1
finish
