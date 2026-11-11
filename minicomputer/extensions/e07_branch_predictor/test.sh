#!/usr/bin/env bash
# e07 tests: branch predictors (always-taken, btfn, 1-bit, 2-bit) in <program>_<pid>.stats
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
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/nested.txt "$HERE"/nested_data.byte "$dir"/ &&
        (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
}

pred() {    # pred <stats file> <predictor>: "mispredictions accuracy"
    awk -v P="$2" '$1 == P && NF == 4 { print $3, $4 }' "$1"
}

branch() {  # branch <stats file> <instruction index>: "executed taken always btfn 1-bit 2-bit"
    awk -v I="$2" 'f && $1 == I { print $(NF-5), $(NF-4), $(NF-3), $(NF-2), $(NF-1), $NF }
                   /^instr/ { f = 1 }' "$1"
}

word_at() { # word_at <file> <byte address>: signed 32-bit little-endian word from a .out file
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

echo "sum_array (9 elements): BEQ .exit (forward) NT x9 then T; BAL .loopback (backward) T x9"
run sa sum_array.txt sum_array_data.byte
S="$WORK/sa/sum_array_1.stats"
check "19 branches, 10 taken, 9 not taken" "$(sed -n 2p "$S" | tr -s ' ')" "branches 19 taken 10 not taken 9"
check "always-taken misses the 9 not-taken BEQs" "$(pred "$S" always-taken)" "9 52.63%"
check "btfn misses only the final BEQ" "$(pred "$S" btfn)" "1 94.74%"
check "1-bit: BEQ exit + first BAL" "$(pred "$S" 1-bit)" "2 89.47%"
check "2-bit: BEQ exit + first BAL" "$(pred "$S" 2-bit)" "2 89.47%"
check "row BEQ .exit (instr 5)" "$(branch "$S" 5)" "10 1 9 1 1 1"
check "row BAL .loopback (instr 10)" "$(branch "$S" 10)" "9 9 0 0 1 1"
check "summary line printed" "$(grep -c '^\[bpred\] pid 1 (sum_array): 19 branches, mispredicted (accuracy): always-taken 9 (52.63%), btfn 1 (94.74%), 1-bit 2 (89.47%), 2-bit 2 (89.47%) -> sum_array_1.stats$' "$WORK/sa/run.txt")" 1
check "program result unchanged (45)" "$(word_at "$WORK/sa/sum_array_data_1.out" 36)" 45

echo "nested.txt: inner BNE T,T,T,T,NT x4 outer iterations; outer BNE T,T,T,NT"
run nest nested.txt nested_data.byte
S="$WORK/nest/nested_1.stats"
check "24 branches, 19 taken, 5 not taken" "$(sed -n 2p "$S" | tr -s ' ')" "branches 24 taken 19 not taken 5"
check "always-taken: the 5 loop exits" "$(pred "$S" always-taken)" "5 79.17%"
check "btfn = always-taken (both branches backward)" "$(pred "$S" btfn)" "5 79.17%"
check "1-bit: 2 per inner loop run (entry + exit) + 2 outer" "$(pred "$S" 1-bit)" "10 58.33%"
check "2-bit: inner 2+1+1+1 + outer 2" "$(pred "$S" 2-bit)" "7 70.83%"
check "row BNE .inner (instr 6)" "$(branch "$S" 6)" "20 16 4 4 8 5"
check "row BNE .outer (instr 9)" "$(branch "$S" 9)" "4 3 1 1 2 2"
check "program result [0] = 20" "$(word_at "$WORK/nest/nested_data_1.out" 0)" 20

echo "counter.txt: bottom-tested loop BNE .loop T x9 then NT"
run cnt counter.txt counter_data.byte
S="$WORK/cnt/counter_1.stats"
check "always-taken / btfn / 1-bit / 2-bit" \
      "$(pred "$S" always-taken)|$(pred "$S" btfn)|$(pred "$S" 1-bit)|$(pred "$S" 2-bit)" \
      "1 90.00%|1 90.00%|2 80.00%|2 80.00%"

echo "5 x nested at once (NP = 4): tables are per processor and reset when a task is loaded"
run five nested.txt nested_data.byte nested.txt nested_data.byte nested.txt nested_data.byte \
         nested.txt nested_data.byte nested.txt nested_data.byte
check "pid 5 had to wait for a processor" "$(grep -c 'pid 5 is waiting' "$WORK/five/run.txt")" 1
for pid in 1 2 3 4 5; do
    S="$WORK/five/nested_$pid.stats"
    check "pid $pid: 1-bit 10, 2-bit 7 (same as a single run)" "$(pred "$S" 1-bit)|$(pred "$S" 2-bit)" "10 58.33%|7 70.83%"
done

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
