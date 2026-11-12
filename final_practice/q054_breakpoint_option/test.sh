#!/usr/bin/env bash
# Q054 checks: -k N prints the non-zero registers the first time instruction N runs in each task.
. "$(dirname "$0")/../lib.sh"
build_solution
brk() { grep "^\[break\] pid $1," "$RUN/run.txt"; }
check "-k 10 with sum_array and array_add" "$(run_sim -b -k 10 sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte)" 0
check "one line per task" "$(grep -c '^\[break\]' "$RUN/run.txt")" 2
check "sum_array: first BAL, after one pass of the loop" "$(brk 1)" "[break] pid 1, instruction 10 (BAL -6), registers before it runs: x1=1 x2=1 x3=4 x4=9 x6=1 x15=-9"
check "array_add: vector registers too (v3 not computed yet)" "$(brk 2)" "[break] pid 2, instruction 10 (v3 = v1 + v2), registers before it runs: x1=16 x2=4 x3=64 x4=68 x5=132 x7=-16 v1=[1 2 3 4 5 6 7 8] v2=[100 101 102 103 104 105 106 107]"
check "-k 8: shown once although it runs 9 times" "$(run_sim -b -k 8 sum_array.txt sum_array_data.byte),$(grep -c '^\[break\]' "$RUN/run.txt")" "0,1"
check "  before x2 = x2 + x6 runs, x2 is still 0" "$(brk 1)" "[break] pid 1, instruction 8 (x2 = x2 + x6), registers before it runs: x3=4 x4=9 x6=1 x15=-9"
check "-k 200 (never reached): no output" "$(run_sim -b -k 200 sum_array.txt sum_array_data.byte),$(grep -c '^\[break\]' "$RUN/run.txt")" "0,0"
check "-k 300 (no such instruction) is rejected" "$(run_sim -b -k 300 sum_array.txt sum_array_data.byte)" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
