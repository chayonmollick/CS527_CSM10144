#!/usr/bin/env bash
# Q046 checks: every task prints a table of its executed instructions per opcode.
. "$(dirname "$0")/../lib.sh"
build_solution
row() {   # row <pid> <opcode in hex, or "total">: the count in that task's table
    awk -v pid="$1" -v key="$2" '/^\[/ { mine = ($1 == "[opcodes]" && $3 == pid) }
                                 mine && $1 == key { print $NF }' "$RUN/run.txt"
}
rows() {  # rows <pid>: how many different opcodes the task executed
    awk -v pid="$1" '/^\[/ { mine = ($1 == "[opcodes]" && $3 == pid) }
                     mine && $1 ~ /^[0-9A-F][0-9A-F]$/ { n++ } END { print n + 0 }' "$RUN/run.txt"
}
check "sum_array and array_add run together" "$(run_sim -b sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte)" 0
check "one table per task" "$(grep -c '^\[opcodes\] pid' "$RUN/run.txt")" 2
check "sum_array: x1 = 0, x2 = 0, x3 = 0 (movc 0F)" "$(row 1 0F)" 3
check "sum_array: 1 + 9 loads (05)" "$(row 1 05)" 10
check "sum_array: compare and BEQ 10 times each (02, 10)" "$(row 1 02),$(row 1 10)" "10,10"
check "sum_array: x3 + 4 and x1 + 1, 9 times each (addi 09)" "$(row 1 09)" 18
check "sum_array: add 9, BAL 9 (01, 1E)" "$(row 1 01),$(row 1 1E)" "9,9"
check "sum_array: one store, one halt (06, 00)" "$(row 1 06),$(row 1 00)" "1,1"
check "sum_array: 9 different opcodes" "$(rows 1)" 9
check "sum_array: total 71 = 4 + 2 x 10 + 5 x 9 + 2" "$(row 1 total)" 71
check "array_add: 4 vector loads, 2 vector adds, 2 vector stores" "$(row 2 25),$(row 2 21),$(row 2 26)" "4,2,2"
check "array_add: BGE 3 times (1A), total 31" "$(row 2 1A),$(row 2 total)" "3,31"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
