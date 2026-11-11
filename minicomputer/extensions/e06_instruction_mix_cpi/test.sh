#!/usr/bin/env bash
# e06 tests: instruction mix, estimated cycles and CPI in <program>_<pid>.stats
#   ./test.sh          builds ./sim (make -s), runs the programs in a scratch directory
# Expected numbers are derived by hand in README.md ("Expected output").
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
SIM="$HERE/sim"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <test dir> <sim arguments...>   (batch mode, fresh directory)
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/mix.txt "$HERE"/mix_data.byte "$dir"/ &&
        (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
}

row() {     # row <stats file> <class>: "count cycles" of that row (cycles = last field)
    awk -v C="$2" '$1 == C { print $2, $NF }' "$1"
}

cpi() { awk '/^CPI/ { print $NF }' "$1"; }

word_at() { # word_at <file> <byte address>: signed 32-bit little-endian word from a .out file
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

echo "sum_array (9 elements): 4 + 10x2 + 9x5 + 1 + halt = 71 instructions"
run sa sum_array.txt sum_array_data.byte
S="$WORK/sa/sum_array_1.stats"
check "alu: 3 inits + 10 compares + 9x3 adds" "$(row "$S" alu)" "40 40"
check "load: size + 9 elements (3 cycles each)" "$(row "$S" load)" "10 30"
check "store: 1" "$(row "$S" store)" "1 3"
check "branch_taken: 9 BAL + the final BEQ (1+2 cycles)" "$(row "$S" branch_taken)" "10 30"
check "branch_not_taken: BEQ 9 times" "$(row "$S" branch_not_taken)" "9 9"
check "halt" "$(row "$S" halt)" "1 1"
check "no mul/div/vector/print" "$(row "$S" mul)|$(row "$S" div)|$(row "$S" vector)|$(row "$S" vector_mem)|$(row "$S" print)" "0 0|0 0|0 0|0 0|0 0"
check "total: 71 instructions, 113 cycles" "$(row "$S" total)" "71 113"
check "CPI 113/71" "$(cpi "$S")" "1.59"
check "total = instructions reported by the OS" "$(row "$S" total | cut -d' ' -f1)" \
      "$(sed -n 's/.*pid 1 (sum_array) finished.* after \([0-9]*\) instructions.*/\1/p' "$WORK/sa/run.txt")"
check "one-line summary printed" "$(grep -c '^\[stats\] pid 1 (sum_array): 71 instructions, 113 cycles, CPI 1.59 -> sum_array_1.stats$' "$WORK/sa/run.txt")" 1
check "data result unchanged (45)" "$(word_at "$WORK/sa/sum_array_data_1.out" 36)" 45

echo "mix.txt: every class once (alu 5, vector 2, vector_mem 2), x8 = 99 skipped"
run mix mix.txt mix_data.byte
S="$WORK/mix/mix_1.stats"
check "alu 5" "$(row "$S" alu)" "5 5"
check "mul 1, div 1" "$(row "$S" mul)|$(row "$S" div)" "1 3|1 10"
check "load 1, store 1" "$(row "$S" load)|$(row "$S" store)" "1 3|1 3"
check "branch taken 1, not taken 1" "$(row "$S" branch_taken)|$(row "$S" branch_not_taken)" "1 3|1 1"
check "vector 2 (4 cycles), vector_mem 2 (8 cycles)" "$(row "$S" vector)|$(row "$S" vector_mem)" "2 8|2 16"
check "print 1, halt 1" "$(row "$S" print)|$(row "$S" halt)" "1 1|1 1"
check "total: 17 instructions, 54 cycles" "$(row "$S" total)" "17 54"
check "CPI 54/17" "$(cpi "$S")" "3.18"
check "program result [0] = 12*4 + 12/4 = 51" "$(word_at "$WORK/mix/mix_data_1.out" 0)" 51
check "vector result [64] = 1 + 3*1, [92] = 8 + 3*8" "$(word_at "$WORK/mix/mix_data_1.out" 64),$(word_at "$WORK/mix/mix_data_1.out" 92)" "4,32"

echo "5 tasks at once (NP = 4): counters are per processor and cleared when a task is loaded"
run multi fir.txt fir_data.byte counter.txt counter_data.byte sum_array.txt sum_array_data.byte \
          mix.txt mix_data.byte sum_array.txt sum_array_data.byte
check "pid 5 had to wait for a processor" "$(grep -c 'pid 5 is waiting' "$WORK/multi/run.txt")" 1
S="$WORK/multi/fir_1.stats"
check "fir: vector 30, vector_mem 18" "$(row "$S" vector)|$(row "$S" vector_mem)" "30 120|18 144"
check "fir: alu 29, load 9, mul 1" "$(row "$S" alu)|$(row "$S" load)|$(row "$S" mul)" "29 29|9 27|1 3"
check "fir: taken 3, not taken 2" "$(row "$S" branch_taken)|$(row "$S" branch_not_taken)" "3 9|2 2"
check "fir: 93 instructions, 335 cycles, CPI 3.60" "$(row "$S" total) $(cpi "$S")" "93 335 3.60"
S="$WORK/multi/counter_2.stats"
check "counter: print 10, alu 21, taken 9" "$(row "$S" print)|$(row "$S" alu)|$(row "$S" branch_taken)" "10 10|21 21|9 27"
check "counter: 43 instructions, 63 cycles" "$(row "$S" total)" "43 63"
check "sum_array pid 3 = single run" "$(row "$WORK/multi/sum_array_3.stats" total)" "71 113"
check "mix pid 4 = single run" "$(row "$WORK/multi/mix_4.stats" total)" "17 54"
check "sum_array pid 5 (reused processor) = single run" "$(row "$WORK/multi/sum_array_5.stats" total)" "71 113"
check "5 .stats files" "$(ls "$WORK/multi" | grep -c '\.stats$')" 5

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
