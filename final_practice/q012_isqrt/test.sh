#!/usr/bin/env bash
# Q012 checks: integer square root (Newton) and the remainder n - r*r.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on isqrt_data (n = 2000000)" "$(run_sim -b solution.txt isqrt_data.byte)" 0
check "r = 1414" "$(word_at "$RUN/isqrt_data_1.out" 4)" 1414
check "n - r*r = 604" "$(word_at "$RUN/isqrt_data_1.out" 8)" 604
check "runs on isqrt_data2 (n = 9801 = 99^2)" "$(run_sim -b solution.txt isqrt_data2.byte)" 0
check "perfect square: r = 99" "$(word_at "$RUN/isqrt_data2_1.out" 4)" 99
check "perfect square: remainder 0" "$(word_at "$RUN/isqrt_data2_1.out" 8)" 0
finish
