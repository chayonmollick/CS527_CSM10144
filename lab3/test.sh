#!/usr/bin/env bash
# Lab 3 tests: build ./sim, run the examples in a fresh temporary directory, check the results.
#   ./test.sh        (or: make test)
# The array-add and FIR results are compared with the same computation done here in awk
# from the data file. Prints one line per check, then "N passed, M failed"; exit code 0 = all passed.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
SIM="$HERE/sim"
EX="$HERE/examples"
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

# words <file> <byte address> <count>: that many consecutive words, comma-separated
words() {
    awk -v A="$2" -v C="$3" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { for (k = 0; k < C; k++) { a = A + 4 * k
                  v = b[a] + b[a+1] * 256 + b[a+2] * 65536 + b[a+3] * 16777216
                  if (v >= 2147483648) v -= 4294967296
                  printf "%s%d", (k ? "," : ""), v }
              print "" }' "$1"
}

# expected <array_add|fir> <data file>: what the program must produce, computed in awk
expected() {
    awk -v WHAT="$1" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        function w(a,  v) { v = b[a] + b[a+1] * 256 + b[a+2] * 65536 + b[a+3] * 16777216
            return v >= 2147483648 ? v - 4294967296 : v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { N = w(0)
              if (WHAT == "array_add")                     # C[i] = A[i] + B[i]
                  for (i = 0; i < N; i++) printf "%s%d", (i ? "," : ""), w(4 + 4 * i) + w(4 + 4 * N + 4 * i)
              else                                          # the handout FIR pseudo code
                  for (k = 0; k < N - 8; k++) { s = 0
                      for (i = 0; i < 8; i++) s += w(36 + 4 * (k + i)) * w(4 + 4 * i)
                      printf "%s%d", (k ? "," : ""), s }
              print "" }' "$2"
}

# gen <array_add|fir> <N>: a data file in the Lab 3 test layout, with negative numbers too
gen() {
    awk -v WHAT="$1" -v N="$2" '
        function w(v) { if (v < 0) v += 4294967296
            printf "%02X %02X %02X %02X\n", v % 256, int(v / 256) % 256, int(v / 65536) % 256, int(v / 16777216) % 256 }
        BEGIN { w(N)
            if (WHAT == "array_add") { for (i = 0; i < N; i++) w(7 * i - 100); for (i = 0; i < N; i++) w(1000 - 3 * i * i) }
            else { for (i = 0; i < 8; i++) w(i % 3 - 1 + 2 * i); for (j = 0; j < N; j++) w((j * 29) % 17 - 8) } }'
}

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <test dir> <sim arguments...>: run in a fresh copy of examples/, print the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$dir"/ && (cd "$dir" && "$SIM" "$@" > run.txt 2>&1)
    echo $?
}

echo "Build"
build_out="$(make -s -B --no-print-directory -C "$HERE" sim 2>&1)"
check "make builds ./sim (-std=c++17 -O2 -Wall -Wextra)" "$?" 0
check "no compiler warnings" "$(printf '%s\n' "$build_out" | grep -ci warning)" 0
[ -x "$SIM" ] || { echo "no ./sim to test"; exit 1; }

echo "Lab 2 base: the handout's sum-of-array program"
check "sum_array runs to the end (exit code 0)" "$(run l2 sum_array.txt sum_array_data.byte)" 0
expected_code="0F 01 00 00|0F 02 00 00|0F 03 00 00|05 04 00 03|02 0F 01 04|10 00 00 06|09 03 03 04|05 06 00 03|01 02 02 06|09 01 01 01|1E 00 00 FA|06 03 00 02|00 00 00 00"
check "program.byte = the handout's listing (+ end of program)" "$(paste -sd'|' "$WORK/l2/program.byte")" "$expected_code"
check "sum 1+..+9 = 45 stored at [x3] = 36" "$(word_at "$WORK/l2/sum_array_data.out" 36)" 45
mkdir -p "$WORK/byte" && cp "$WORK/l2/program.byte" "$EX/sum_array_data.byte" "$WORK/byte/"
(cd "$WORK/byte" && "$SIM" program.byte sum_array_data.byte result.byte > run.txt 2>&1)
check "./sim program.byte data.byte out.byte gives the same result" "$(word_at "$WORK/byte/result.byte" 36)" 45
printf 'Read x1, 0\nx2 = x1 * 3\nWrite x2, 8\n' > "$WORK/legacy.txt"
(cd "$WORK" && "$SIM" -c legacy.txt legacy.byte > /dev/null)
check "legacy Read/Write compile to 0D / 0E" "$(paste -sd'|' "$WORK/legacy.byte")" "0D 01 00 00|0B 02 01 03|0E 08 00 02|00 00 00 00"

echo "Lab 3 compiler: vector statements -> byte code"
cat > "$WORK/vcode.txt" << 'EOF'
v3 = v1 + v4;
v3 = v1 + 15;
v3 = v1 * v4;
v3 = v1 * x4;
v1 = [x2]
v1 = [32]
[x2] = v1
[32] = v1
v3 = v1 - v4
v3 = v1 - 15
v3 = v1 * 15
v3 = v1 + x4
v3 = v1 - x4
v2 = v1
EOF
(cd "$WORK" && "$SIM" -c vcode.txt vcode.byte > /dev/null)
check "the handout's vector examples (21 29 23 33 25 2C 26 2E)" "$(head -8 "$WORK/vcode.byte" | paste -sd'|' -)" \
      "21 03 01 04|29 03 01 0F|23 03 01 04|33 03 01 04|25 01 00 02|2C 01 00 20|26 02 00 01|2E 20 00 01"
check "the other forms (22 2A 2B 31 32, copy = + 0)" "$(sed -n '9,15p' "$WORK/vcode.byte" | paste -sd'|' -)" \
      "22 03 01 04|2A 03 01 0F|2B 03 01 0F|31 03 01 04|32 03 01 04|29 02 01 00|00 00 00 00"

echo "Lab 3 processor: every vector instruction (examples/vector_ops.txt)"
check "vector_ops runs to the end" "$(run vo vector_ops.txt vector_ops_data.byte)" 0
VO="$WORK/vo/vector_ops_data.out"
check "21 v1 + v2   (written by 2E [32] = v3)" "$(words "$VO" 32 8)" "2,4,6,8,10,12,14,16"
check "22 v3 - v1   (2E [64] = v4)" "$(words "$VO" 64 8)" "1,2,3,4,5,6,7,8"
check "23 v1 * v2   (26 [x2] = v5)" "$(words "$VO" 96 8)" "1,4,9,16,25,36,49,64"
check "29 v1 + 15" "$(words "$VO" 128 8)" "16,17,18,19,20,21,22,23"
check "2A v1 - 10   (negative lanes)" "$(words "$VO" 160 8)" "-9,-8,-7,-6,-5,-4,-3,-2"
check "2B v1 * 3" "$(words "$VO" 192 8)" "3,6,9,12,15,18,21,24"
check "31 v1 + x4" "$(words "$VO" 224 8)" "6,7,8,9,10,11,12,13"
check "32 v1 - x4" "$(words "$VO" 256 8)" "-4,-3,-2,-1,0,1,2,3"
check "33 v1 * x4" "$(words "$VO" 288 8)" "5,10,15,20,25,30,35,40"
check "copy v12 = v11" "$(words "$VO" 320 8)" "5,10,15,20,25,30,35,40"
check "vector operations leave the flags alone" "$(word_at "$VO" 352)" 1
check "the input (25/2C reads) is unchanged" "$(words "$VO" 0 8)" "1,2,3,4,5,6,7,8"

echo "Lab 3 test program 1: array add (examples/array_add.txt)"
check "array_add runs to the end" "$(run aa array_add.txt array_add_data.byte)" 0
check "N = 16: C = A + B, all 16 elements" "$(words "$WORK/aa/array_add_data.out" 132 16)" "$(expected array_add "$EX/array_add_data.byte")"
check "N = 16 takes 27 instructions (9 per 8 elements + setup)" "$(grep -o 'after [0-9]* instructions' "$WORK/aa/run.txt")" "after 27 instructions"
mkdir -p "$WORK/aa40" && gen array_add 40 > "$WORK/aa40/add40.byte"
(cd "$WORK/aa40" && "$SIM" "$EX/array_add.txt" add40.byte > run.txt 2>&1)
check "N = 40 with negative numbers: all 40 elements" "$(words "$WORK/aa40/add40.out" 324 40)" "$(expected array_add "$WORK/aa40/add40.byte")"
check "N = 40: A and B are unchanged" "$(words "$WORK/aa40/add40.out" 4 3),$(words "$WORK/aa40/add40.out" 164 3)" "-100,-93,-86,1000,997,988"

echo "Lab 3 test program 2: FIR filter (examples/fir.txt)"
check "fir runs to the end" "$(run fir fir.txt fir_data.byte)" 0
check "N = 24: the 16 outputs from 0x100 match the pseudo code" "$(words "$WORK/fir/fir_data.out" 256 16)" "$(expected fir "$EX/fir_data.byte")"
check "N = 64 (largest; output overlaps the input end): all 56 outputs" \
      "$(run fir64 fir.txt fir64_data.byte > /dev/null; words "$WORK/fir64/fir64_data.out" 256 56)" "$(expected fir "$EX/fir64_data.byte")"
mkdir -p "$WORK/fir16" && gen fir 16 > "$WORK/fir16/fir16.byte"
(cd "$WORK/fir16" && "$SIM" "$EX/fir.txt" fir16.byte > run.txt 2>&1)
check "N = 16 (one block of 8 outputs)" "$(words "$WORK/fir16/fir16.out" 256 8)" "$(expected fir "$WORK/fir16/fir16.byte")"
mkdir -p "$WORK/fir8" && gen fir 8 > "$WORK/fir8/fir8.byte"
(cd "$WORK/fir8" && "$SIM" "$EX/fir.txt" fir8.byte > run.txt 2>&1)
check "N = 8: no output (k < N-8 never holds), memory unchanged" "$(cmp -s "$WORK/fir8/fir8.byte" "$WORK/fir8/fir8.out" && echo same)" same

echo "Errors"
mkdir -p "$WORK/err" && cd "$WORK/err" && printf '00 00 00 00\n' > d.byte
printf 'x1 = x300 + 1\n' > badreg.txt
"$SIM" badreg.txt d.byte > out.txt 2>&1; rc=$?
check "compile error names file and line, exit code 1" "$(grep -c 'badreg.txt:1: register x300 does not exist' out.txt),$rc" "1,1"
printf 'v32 = v1 + v2\n' > badv.txt
check "v32 does not exist" "$("$SIM" badv.txt d.byte 2>&1 | grep -c 'register v32 does not exist')" 1
printf 'v1 = v2 / v3\n' > vdiv.txt
check "vector division is rejected (+ - * only)" "$("$SIM" vdiv.txt d.byte 2>&1 | grep -c 'vector division is not supported')" 1
awk 'BEGIN { for (i = 0; i < 64; i++) print "x1 = x1 + 1" }' > long.txt
check "64 statements do not fit in Instruction[256]" "$("$SIM" long.txt d.byte 2>&1 | grep -c 'program has 64 instructions')" 1
printf 'x1 = 5\nx2 = 0\nx3 = x1 / x2\n' > div0.txt
"$SIM" div0.txt d.byte > out.txt 2>&1; rc=$?
check "division by zero stops the program, exit code 1" "$(grep -c 'error at instruction 2: division by zero' out.txt),$rc" "1,1"
printf 'x1 = 255\nx1 = x1 * 16\nv1 = [x1]\n' > far.txt
check "vector read of 4080..4111 runs past Data[4096]: error" "$("$SIM" far.txt d.byte 2>&1 | grep -c 'address 4096 is outside Data')" 1
printf '40 00 00 00\n' > bad.byte
check "unknown opcode in a .byte program is an error" "$("$SIM" bad.byte d.byte 2>&1 | grep -c 'unknown opcode 0x40')" 1
check "branch to an unknown label is a compile error" "$(printf 'BNE .nowhere\n' > nolabel.txt; "$SIM" nolabel.txt d.byte 2>&1 | grep -c 'unknown label .nowhere')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
