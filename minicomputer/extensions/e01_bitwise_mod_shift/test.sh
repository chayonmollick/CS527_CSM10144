#!/usr/bin/env bash
# e01 tests: build, run the example programs in batch mode in a temp directory, check the results.
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

echo "e01: gcd.txt (Euclid with mod)"
check "gcd exit code" "$(run gcd gcd.txt gcd_data.byte)" 0
check "gcd(1071, 462) = 21" "$(word_at "$WORK/gcd/gcd_data_1.out" 8)" 21
check "gcd.freq: test 4 times, loop body 3 times" "$(cut -f1 "$WORK/gcd/gcd_1.freq" | paste -sd, -)" "1,1,.loop,4,4,3,3,3,3,.done,1"
mkdir -p "$WORK/gcd2" && printf 'B4 00 00 00\n30 00 00 00\n' > "$WORK/gcd2/d.byte"
run gcd2 gcd.txt d.byte > /dev/null
check "gcd(180, 48) = 12" "$(word_at "$WORK/gcd2/d_1.out" 8)" 12

echo "e01: popcount.txt (& 1 and >>)"
run pop popcount.txt popcount_data.byte > /dev/null
check "popcount(0x12345678) = 13" "$(word_at "$WORK/pop/popcount_data_1.out" 4)" 13
mkdir -p "$WORK/pop2" && printf 'FF FF FF FF\n' > "$WORK/pop2/d.byte"
run pop2 popcount.txt d.byte > /dev/null
check "popcount(-1) = 32 (arithmetic >> still ends: fixed 32 steps)" "$(word_at "$WORK/pop2/d_1.out" 4)" 32
mkdir -p "$WORK/pop3" && printf '00 00 00 80\n' > "$WORK/pop3/d.byte"
run pop3 popcount.txt d.byte > /dev/null
check "popcount(0x80000000) = 1" "$(word_at "$WORK/pop3/d_1.out" 4)" 1

echo "e01: evenodd.txt (& 1 on positive and negative numbers)"
run eo evenodd.txt evenodd_data.byte > /dev/null
check "evens in 3 8 -5 0 12 7 -4 = 4 (at 4N+4)" "$(word_at "$WORK/eo/evenodd_data_1.out" 32)" 4
check "odds = 3 (at 4N+8)" "$(word_at "$WORK/eo/evenodd_data_1.out" 36)" 3

echo "e01: bitops.txt (every operator, register and constant forms)"
check "bitops exit code" "$(run ops bitops.txt bitops_data.byte)" 0
OUT="$WORK/ops/bitops_data_1.out"
check "-100 & 7 = 4"            "$(word_at "$OUT" 8)" 4
check "-100 | 7 = -97"          "$(word_at "$OUT" 12)" -97
check "-100 ^ 7 = -101"         "$(word_at "$OUT" 16)" -101
check "7 << 7 = 896"            "$(word_at "$OUT" 20)" 896
check "-100 >> 7 = -1 (arithmetic)" "$(word_at "$OUT" 24)" -1
check "-100 mod 7 = -2 (sign of the dividend)" "$(word_at "$OUT" 28)" -2
check "-100 & 255 = 156"        "$(word_at "$OUT" 32)" 156
check "7 | 8 = 15"              "$(word_at "$OUT" 36)" 15
check "-100 ^ 255 = -157"       "$(word_at "$OUT" 40)" -157
check "7 << 28 = 0x70000000"    "$(word_at "$OUT" 44)" 1879048192
check "-100 >> 2 = -25"         "$(word_at "$OUT" 48)" -25
check "100 mod 7 = 2"           "$(word_at "$OUT" 52)" 2
check "-100 << 40 = 0"          "$(word_at "$OUT" 56)" 0
check "-100 >> 40 = -1"         "$(word_at "$OUT" 60)" -1
check "& does not change the flags (BEQ not taken)" "$(word_at "$OUT" 64)" 1

echo "e01: encodings (./sim -c)"
mkdir -p "$WORK/enc" && cd "$WORK/enc" || exit 1
printf 'x1=x2<<3\nx1 = x2 >> x3\nx4 = x5 & 1\nx4 = x5|x6\nx7 = x8 ^ 255\nx9 = x10 mod 10\nx9 = x10 MOD x11\n' > enc.txt
"$SIM" -c enc.txt > listing.txt 2>&1
check "byte code of the 7 statements" "$(paste -sd'|' program.byte)" \
      "49 01 02 03|4A 01 02 03|43 04 05 01|44 04 05 06|47 07 08 FF|41 09 0A 0A|40 09 0A 0B|00 00 00 00"
check "listing disassembles mod" "$(grep -c 'x9 = x10 mod x11' listing.txt)" 1
printf 'x1 = 10\nx2 = x1 %% 3\n[0] = x2\n' > pct.txt
printf '00 00 00 00\n' > d.byte
"$SIM" -b pct.txt d.byte > /dev/null 2>&1
check "% is still a comment: 'x2 = x1 % 3' is x2 = x1" "$(word_at d_1.out 0)" 10

echo "e01: errors"
printf 'x1 = 5\nx2 = 0\nx3 = x1 mod x2\n' > mod0.txt
"$SIM" -b mod0.txt d.byte > out.txt 2>&1; rc=$?
check "runtime error: modulus by zero stops the task (exit code 1)" "$(grep -c 'modulus by zero' out.txt),$rc" "1,1"
printf 'v1 = v2 & v3\n' > vec.txt
check "compile error: & on vector registers" "$("$SIM" -b vec.txt d.byte 2>&1 | grep -c "vec.txt:1: '&' works on x registers only")" 1
printf 'x1 = x2 < 3\n' > lt.txt
check "compile error: a single < is not an operator" "$("$SIM" -b lt.txt d.byte 2>&1 | grep -c "unknown operator '<'")" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
