#!/usr/bin/env bash
# e05 tests: build, run the example programs in batch mode in a temp directory, check the results.
#   ./test.sh          (also run by "make test", followed by the 33 base checks)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
SIM="$HERE/sim"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

# word_at <file> <byte address>: signed 32-bit little-endian word from a .byte/.out file
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

# hexword <decimal>: one .byte line for a 32-bit word, e.g. hexword -1 -> "FF FF FF FF"
hexword() {
    local v=$1; [ "$v" -lt 0 ] && v=$((v + 4294967296))
    printf '%02X %02X %02X %02X\n' $((v & 255)) $(((v >> 8) & 255)) $(((v >> 16) & 255)) $(((v >> 24) & 255))
}

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <test dir> <sim arguments...>: copy the examples there, run in batch mode, print the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && rm -f "$dir/log.txt" \
        && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

echo "e05: dot.txt (vector multiply-add, then one sum)"
check "dot exit code" "$(run dot dot.txt dot_data.byte)" 0
check "(1..16) . (16..1) = 816 (at 4 + 8N)" "$(word_at "$WORK/dot/dot_data_1.out" 132)" 816
check "print v3 logs the 8 partial sums" "$(grep -c 'v3 : 00000058 00000064 0000006C 00000070 00000070 0000006C 00000064 00000058$' "$WORK/dot/log.txt")" 1
check "print x8 = 0x330" "$(grep -c 'x8 : 00000330' "$WORK/dot/log.txt")" 1
mkdir -p "$WORK/dot8" && { hexword 8; for i in 1 2 3 4 5 6 7 8; do hexword $i; done; for i in 1 2 3 4 5 6 7 8; do hexword -1; done; } > "$WORK/dot8/d.byte"
run dot8 dot.txt d.byte > /dev/null
check "N = 8: (1..8) . (-1, ..., -1) = -36" "$(word_at "$WORK/dot8/d_1.out" 68)" -36

echo "e05: fir_sum.txt (FIR with sum) against fir_lab3.txt (Lab 3 FIR)"
check "fir_sum exit code" "$(run fir fir_sum.txt fir_data.byte fir_lab3.txt fir_data.byte)" 0
check "out[0] = 204" "$(word_at "$WORK/fir/fir_data_1.out" 256)" 204
check "out[15] = 744" "$(word_at "$WORK/fir/fir_data_1.out" 316)" 744
check "all 16 outputs equal the Lab 3 FIR" "$(sed -n 65,80p "$WORK/fir/fir_data_1.out" | paste -sd'|' -)" "$(sed -n 65,80p "$WORK/fir/fir_data_2.out" | paste -sd'|' -)"
check "program size: 17 instructions (Lab 3 FIR: 53)" "$(grep -c 'fir_sum.txt fir_data.byte (17 instructions)' "$WORK/fir/run.txt"),$(grep -c 'fir_lab3.txt fir_data.byte (53 instructions)' "$WORK/fir/run.txt")" "1,1"
check "executed: 169 instructions (Lab 3 FIR: 93)" "$(grep -c 'fir_sum) finished.* after 169 instructions' "$WORK/fir/run.txt"),$(grep -c 'fir_lab3) finished.* after 93 instructions' "$WORK/fir/run.txt")" "1,1"

echo "e05: relu.txt (max three ways, broadcast, sum, print v)"
check "relu exit code" "$(run relu relu.txt relu_data.byte)" 0
OUT="$WORK/relu/relu_data_1.out"
check "sum of max(a, 0) = 54"              "$(word_at "$OUT" 68)" 54
check "sum of max(a, 5), scalar x3 = 99"   "$(word_at "$OUT" 72)" 99
check "sum of max(a, 5), broadcast v7 = 99" "$(word_at "$OUT" 76)" 99
check "ReLU in place: 3 -2 7 -> 3 0 7"     "$(word_at "$OUT" 4),$(word_at "$OUT" 8),$(word_at "$OUT" 12)" "3,0,7"
check "print v4" "$(grep -c 'v4 : 00000003 00000009 00000007 00000005 0000000A 0000000C 00000002 00000006$' "$WORK/relu/log.txt")" 1
P="$WORK/relu/program.byte"
check "v7 = x3 (broadcast) = 27 07 00 03" "$(sed -n 4p "$P")" "27 07 00 03"
check "v2 = v1 max x3 = 3B 02 01 03" "$(sed -n 13p "$P")" "3B 02 01 03"
check "v2 = max v1 v7 (prefix form) = 39 02 01 07" "$(sed -n 15p "$P")" "39 02 01 07"
check "v1 = v1 max 0 = 3A 01 01 00" "$(sed -n 17p "$P")" "3A 01 01 00"
check "print v4 = 28 00 00 04 and x9 = sum v4 = 38 09 00 04" "$(sed -n 23,24p "$P" | paste -sd'|' -)" "28 00 00 04|38 09 00 04"

echo "e05: signed max and sum"
mkdir -p "$WORK/sgn" && cd "$WORK/sgn" || exit 1
printf '00 00 00 00\n' > d.byte
cat > sgn.txt <<'PROG'
x1 = 0
x1 = x1 - 5
v1 = x1           % -5 in every lane
x2 = 3
v2 = x2
v3 = v1 max v2    % signed: 3 (unsigned would pick 0xFFFFFFFB)
x3 = sum v3
[0] = x3          % 24
x4 = sum v1
[4] = x4          % -40
v4 = v1 max 0
x5 = sum v4
[8] = x5          % 0
PROG
"$SIM" -b sgn.txt d.byte > run.txt 2>&1
check "max(-5, 3) = 3 in all lanes: sum 24" "$(word_at d_1.out 0)" 24
check "sum of eight -5 = -40" "$(word_at d_1.out 4)" -40
check "max(-5, 0) = 0" "$(word_at d_1.out 8)" 0

echo "e05: errors"
printf 'x1 = sum x2\n' > e1.txt
check "sum needs a vector" "$("$SIM" -b e1.txt d.byte 2>&1 | grep -c 'e1.txt:1: usage: x<n> = sum v<n>')" 1
printf 'v1 = sum v2\n' > e2.txt
check "sum goes to an x register" "$("$SIM" -b e2.txt d.byte 2>&1 | grep -c 'usage: x<n> = sum v<n>')" 1
printf 'v1 = x2 max x3\n' > e3.txt
check "max works on vectors" "$("$SIM" -b e3.txt d.byte 2>&1 | grep -c 'max works on vectors')" 1
printf 'x1 = max v1 v2\n' > e4.txt
check "max result must be a vector" "$("$SIM" -b e4.txt d.byte 2>&1 | grep -c 'max works on vectors')" 1
printf 'print v40\n' > e5.txt
check "print v40: no such register" "$("$SIM" -b e5.txt d.byte 2>&1 | grep -c 'register v40 does not exist')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
