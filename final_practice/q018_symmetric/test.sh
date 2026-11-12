#!/usr/bin/env bash
# Q018 checks: symmetric 4x4 matrix test, 1 or 0 at 64.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on symmetric_data" "$(run_sim -b solution.txt symmetric_data.byte)" 0
check "symmetric -> 1" "$(word_at "$RUN/symmetric_data_1.out" 64)" 1
check "runs on symmetric_data2" "$(run_sim -b solution.txt symmetric_data2.byte)" 0
check "A[2][3] != A[3][2] (last pair) -> 0" "$(word_at "$RUN/symmetric_data2_1.out" 64)" 0
finish
