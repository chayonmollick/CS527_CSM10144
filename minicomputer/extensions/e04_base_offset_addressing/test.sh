#!/usr/bin/env bash
# e04 tests: build, run the example programs in batch mode in a temp directory, check the results.
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

echo "e04: records.txt ((key, value) records, offsets 0 and 4)"
check "records exit code" "$(run rec records.txt records_data.byte)" 0
check "sum of the values 100+250-7+1000+3 = 1346 (at 8+8N)" "$(word_at "$WORK/rec/records_data_1.out" 48)" 1346
check "value of key 30 = -7 (at 12+8N)" "$(word_at "$WORK/rec/records_data_1.out" 52)" -7
check "x5 = [x2 + 0] = 70 05 00 02" "$(sed -n 8p "$WORK/rec/program.byte")" "70 05 00 02"
check "x6 = [x2 + 4] = 70 06 04 02" "$(sed -n 9p "$WORK/rec/program.byte")" "70 06 04 02"
check "[x2 + 4] = x4 = 71 02 04 04" "$(sed -n 18p "$WORK/rec/program.byte")" "71 02 04 04"
mkdir -p "$WORK/rec99" && sed '2s/.*/63 00 00 00/' "$HERE/records_data.byte" > "$WORK/rec99/d.byte"
run rec99 records.txt d.byte > /dev/null
check "key 99 is not there: 0" "$(word_at "$WORK/rec99/d_1.out" 52)" 0

echo "e04: records_plain.txt (same job with [xA] only)"
run plain records_plain.txt records_data.byte > /dev/null
check "same sum" "$(word_at "$WORK/plain/records_data_1.out" 48)" 1346
check "same value" "$(word_at "$WORK/plain/records_data_1.out" 52)" -7
check "base + offset runs 61 instructions" "$(grep -c 'after 61 instructions' "$WORK/rec/run.txt")" 1
check "plain addressing runs 67 (one add per record + 1)" "$(grep -c 'after 67 instructions' "$WORK/plain/run.txt")" 1

echo "e04: vrecords.txt (vector loads/stores with offsets 0, 32, 64)"
check "vrecords exit code" "$(run vrec vrecords.txt vrecords_data.byte)" 0
OUT="$WORK/vrec/vrecords_data_1.out"
check "record 0: c[0] = 1 + 10"   "$(word_at "$OUT" 68)" 11
check "record 0: c[7] = 8 + 80"   "$(word_at "$OUT" 96)" 88
check "record 1: c[0] = 100 - 1"  "$(word_at "$OUT" 164)" 99
check "record 1: c[7] = 107 - 8"  "$(word_at "$OUT" 192)" 99
check "b of record 0 unchanged"   "$(word_at "$OUT" 36)" 10
check "v2 = [x2 + 32] = 72 02 20 02" "$(sed -n 6p "$WORK/vrec/program.byte")" "72 02 20 02"
check "[x2 + 64] = v3 = 73 02 40 03" "$(sed -n 8p "$WORK/vrec/program.byte")" "73 02 40 03"

echo "e04: addresses above 255 and across a page boundary"
mkdir -p "$WORK/page" && cd "$WORK/page" || exit 1
printf '00 00 00 00\n' > d.byte
cat > cross.txt <<'PROG'
x1 = 255
x1 = x1 + 255     % 510
x2 = 250
x2 = x2 * 250     % 62500
[x1 + 0] = x2     % bytes 510..513: the word crosses into data page 1
x3 = [x1 + 0]
[0] = x3
[x1 + 255] = x2   % 765, the largest offset
x4 = [x1 + 255]
x4 = x4 + 1
[4] = x4
PROG
"$SIM" -b cross.txt d.byte > run.txt 2>&1; rc=$?
check "word across the page boundary reads back" "$rc,$(word_at d_1.out 0)" "0,62500"
check "offset 255" "$(word_at d_1.out 4)" 62501
check "data page 1 was demand-mapped (1 page fault)" "$(grep -c '1 page fault' run.txt)" 1

echo "e04: errors"
printf 'x1 = [x2 + x3]\n' > e1.txt
check "offset must be a constant" "$("$SIM" -b e1.txt d.byte 2>&1 | grep -c 'e1.txt:1: base + offset must be \[x<n> + <constant 0..255>\]')" 1
printf 'x1 = [x2 + 256]\n' > e2.txt
check "offset 256 is out of range" "$("$SIM" -b e2.txt d.byte 2>&1 | grep -c 'constant 256 is out of range 0..255')" 1
printf 'x1 = [4 + 4]\n' > e3.txt
check "base must be an x register" "$("$SIM" -b e3.txt d.byte 2>&1 | grep -c 'base + offset must be')" 1
printf 'x1 = [x2 - 4]\n' > e4.txt
check "only + is allowed" "$("$SIM" -b e4.txt d.byte 2>&1 | grep -c 'expected \[x<n>\], \[<constant>\] or \[x<n> + <constant>\]')" 1
printf '[x1 + 4] = 5\n' > e5.txt
check "a constant cannot be stored" "$("$SIM" -b e5.txt d.byte 2>&1 | grep -c 'only a register can be stored to memory')" 1
printf 'x1 = 255\nx1 = x1 * 16\nx2 = [x1 + 100]\n' > e6.txt
"$SIM" -b e6.txt d.byte > out.txt 2>&1; rc=$?
check "runtime: 4080 + 100 is outside the data space" "$(grep -c "data address 418. is outside the logical data space" out.txt),$rc" "1,1"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
