#!/usr/bin/env bash
# Q052 checks: branch lines of the .freq report get "taken T" and "not taken N" columns.
. "$(dirname "$0")/../lib.sh"
build_solution
cols() {  # cols <freq file> <source text>: the extra columns of that line
    awk -F'\t' -v s="$2" '$2 == s { print $3 "/" $4 }' "$RUN/$1"
}
check "sum_array and counter run together" "$(run_sim -b sum_array.txt sum_array_data.byte counter.txt counter_data.byte)" 0
check "BEQ .exit: 10 = 1 taken + 9 not taken" "$(cols sum_array_1.freq 'BEQ .exit')" "taken 1/not taken 9"
check "BAL .loopback: always taken" "$(cols sum_array_1.freq 'BAL .loopback')" "taken 9/not taken 0"
check "counter's BNE .loop: 9 taken, falls through once" "$(cols counter_2.freq 'BNE .loop')" "taken 9/not taken 1"
check "only the 2 branch lines have extra columns" "$(awk -F'\t' 'NF > 2' "$RUN/sum_array_1.freq" | wc -l | tr -d ' ')" 2
check "first column as in the Lab 6 report" "$(cut -f1 "$RUN/sum_array_1.freq" | paste -sd, -)" "1,1,1,1,.loopback,10,10,9,9,9,9,9,.exit,1"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
