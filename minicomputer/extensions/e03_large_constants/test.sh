#!/usr/bin/env bash
# e03 tests: build, run the example programs in batch mode in a temp directory, check the results.
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

echo "e03: bigsum.txt (100000 + 200000, -5, 0x12345678)"
check "bigsum exit code" "$(run sum bigsum.txt bigsum_data.byte)" 0
OUT="$WORK/sum/bigsum_data_1.out"
check "100000 + 200000 = 300000" "$(word_at "$OUT" 0)" 300000
check "-5 stored as FF FF FF FB (file order FB FF FF FF)" "$(sed -n 2p "$OUT")" "FB FF FF FF"
check "-5 read back" "$(word_at "$OUT" 4)" -5
check "0x12345678 stored as 78 56 34 12" "$(sed -n 3p "$OUT")" "78 56 34 12"
check "255 still works" "$(word_at "$OUT" 12)" 255
check "x1 = 100000 -> MOVC 1, SHLOR 0x86, SHLOR 0xA0" "$(sed -n 1,3p "$WORK/sum/program.byte" | paste -sd'|' -)" "0F 01 00 01|60 01 00 86|60 01 00 A0"
check "x4 = -5 -> MOVC FF + 3 SHLOR" "$(sed -n 9,12p "$WORK/sum/program.byte" | paste -sd'|' -)" "0F 04 00 FF|60 04 00 FF|60 04 00 FF|60 04 00 FB"
check "10 source lines -> 20 instructions + halt" "$(grep -c 'bigsum.txt bigsum_data.byte (21 instructions)' "$WORK/sum/run.txt")" 1
check "bigsum.freq: every line ran once" "$(cut -f1 "$WORK/sum/bigsum_1.freq" | paste -sd, -)" "1,1,1,1,1,1,1,1,1,1"

echo "e03: bigloop.txt (branch offsets and .freq with expanded lines)"
check "bigloop exit code" "$(run loop bigloop.txt bigloop_data.byte)" 0
OUT="$WORK/loop/bigloop_data_1.out"
check "10 x 100000 = 1000000" "$(word_at "$OUT" 0)" 1000000
check "x5 = -1" "$(word_at "$OUT" 4)" -1
check "forward branch skipped the whole 4-instruction line" "$(word_at "$OUT" 8)" 0
check "BNE .loop = 11 00 00 FA (-6)" "$(sed -n 9p "$WORK/loop/program.byte")" "11 00 00 FA"
check "BNE .skip = 11 00 00 06 (+6)" "$(sed -n 12p "$WORK/loop/program.byte")" "11 00 00 06"
check "freq counts lines, not instructions (10, not 30)" "$(cut -f1 "$WORK/loop/bigloop_1.freq" | paste -sd, -)" "1,1,.loop,10,10,10,10,10,1,1,1,0,0,.skip,1,1"

echo "e03: boundary values"
mkdir -p "$WORK/edge" && cd "$WORK/edge" || exit 1
cat > edge.txt <<'PROG'
x1 = 4294967295
[0] = x1
x2 = -2147483648
[4] = x2
x3 = 256
[8] = x3
x4 = 65536
[12] = x4
x5 = 0xFF
[16] = x5
x6 = -0
[20] = x6
x7 = 2147483647
[24] = x7
x8 = -255
[28] = x8
PROG
printf '00 00 00 00\n' > d.byte
"$SIM" -b edge.txt d.byte > run.txt 2>&1
check "4294967295 = -1 (same 32 bits)" "$(word_at d_1.out 0)" -1
check "-2147483648 (smallest)" "$(word_at d_1.out 4)" -2147483648
check "256 = 2 instructions" "$(word_at d_1.out 8),$(sed -n 11,12p program.byte | paste -sd'|' -)" "256,0F 03 00 01|60 03 00 00"
check "65536" "$(word_at d_1.out 12)" 65536
check "0xFF = 255" "$(word_at d_1.out 16)" 255
check "-0 = 0" "$(word_at d_1.out 20)" 0
check "2147483647 (largest positive)" "$(word_at d_1.out 24)" 2147483647
check "-255" "$(word_at d_1.out 28)" -255
check "sizes 4+4+2+3+1+1+4+4 + 8 stores + halt = 32" "$(grep -c '(32 instructions)' run.txt)" 1

echo "e03: errors"
printf 'x1 = 4294967296\n' > big.txt
check "4294967296 does not fit" "$("$SIM" -b big.txt d.byte 2>&1 | grep -c 'big.txt:1: constant 4294967296 does not fit in 32 bits')" 1
printf 'x1 = -2147483649\n' > neg.txt
check "-2147483649 does not fit" "$("$SIM" -b neg.txt d.byte 2>&1 | grep -c 'constant -2147483649 does not fit')" 1
printf 'x1 = 0x123456789\n' > hex.txt
check "9 hex digits do not fit" "$("$SIM" -b hex.txt d.byte 2>&1 | grep -c 'does not fit in 32 bits')" 1
printf 'x1 = x2 + 1000\n' > op.txt
check "operands of + stay 0..255" "$("$SIM" -b op.txt d.byte 2>&1 | grep -c 'constant 1000 is out of range 0..255')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
