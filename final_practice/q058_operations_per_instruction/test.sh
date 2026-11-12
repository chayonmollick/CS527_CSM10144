#!/usr/bin/env bash
# Q058 checks: effective operations (scalar ALU 1, vector ALU 8) and operations per instruction.
. "$(dirname "$0")/../lib.sh"
build_solution
ops() { grep -F "[ops] pid $1 " "$RUN/run.txt"; }
check "sum_array, array_add and scale run together" "$(run_sim -b sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte scale.txt scale_data.byte)" 0
check "sum_array: 10 compares + 9 x 3 additions, scalar only" "$(ops 1)" "[ops] pid 1 (sum_array): 37 operations in 71 instructions = 0.52 per instruction"
check "array_add: 14 scalar + 2 vector adds x 8" "$(ops 2)" "[ops] pid 2 (array_add): 30 operations in 31 instructions = 0.97 per instruction"
check "scale: 2 x (8 + 8 + 1 + 1) in 18 instructions" "$(ops 3)" "[ops] pid 3 (scale): 36 operations in 18 instructions = 2.00 per instruction"
check "scale computed 3x + 1: [0] = 4, [60] = 49" "$(word_at "$RUN/scale_data_3.out" 0),$(word_at "$RUN/scale_data_3.out" 60)" "4,49"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
