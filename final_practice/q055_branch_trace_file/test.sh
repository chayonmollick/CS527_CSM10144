#!/usr/bin/env bash
# Q055 checks: <program>_<pid>.branches lists every executed branch: from -> to, taken or not.
. "$(dirname "$0")/../lib.sh"
build_solution
check "sum_array, counter and complex_mul run together" "$(run_sim -b sum_array.txt sum_array_data.byte counter.txt counter_data.byte complex_mul.txt complex_mul_data.byte)" 0
B="$RUN/sum_array_1.branches"
check "sum_array: 19 branches executed (BEQ 10 + BAL 9)" "$(wc -l < "$B" | tr -d ' ')" 19
check "1st: BEQ (instruction 5, target .exit = 11) not taken" "$(sed -n 1p "$B")" "5 -> 11 not taken"
check "2nd: BAL back to .loopback (10 -> 4)" "$(sed -n 2p "$B")" "10 -> 4 taken"
check "BEQ not taken 9 times, BAL taken 9 times" "$(grep -cx '5 -> 11 not taken' "$B"),$(grep -cx '10 -> 4 taken' "$B")" "9,9"
check "last: BEQ taken, the loop ends" "$(tail -1 "$B")" "5 -> 11 taken"
check "counter: BNE taken 9 times, then falls through" "$(grep -cx '4 -> 1 taken' "$RUN/counter_2.branches"),$(tail -1 "$RUN/counter_2.branches")" "9,4 -> 1 not taken"
check "complex_mul has no branch: empty file" "$(wc -c < "$RUN/complex_mul_3.branches" | tr -d ' ')" 0
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
