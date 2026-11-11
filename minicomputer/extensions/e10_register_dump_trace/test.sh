#!/usr/bin/env bash
# e10 tests: register dump <program>_<pid>.regs and per-task trace <program>_<pid>.trace (-T)
#   ./test.sh          builds ./sim (make -s), runs the programs in a scratch directory
# Expected values are derived by hand in README.md ("Expected output").
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
    mkdir -p "$dir" && cp "$EX"/* "$dir"/ && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
}

xreg() {    # xreg <regs file> <x register>: "decimal hex"
    awk -v R="$2" '$1 == R { print $3, $4 }' "$1"
}

vreg() {    # vreg <regs file> <v register>: the 8 lanes
    awk -v R="$2" '$1 == R { s = $3; for (i = 4; i <= NF; i++) s = s " " $i; print s }' "$1"
}

xlist() {   # xlist <regs file>: every listed x register as name=value, comma separated
    awk '$1 ~ /^x[0-9]+$/ { printf "%s%s=%s", (n++ ? "," : ""), $1, $3 } END { print "" }' "$1"
}

tline() {   # tline <trace file> <step>: that trace line with single spaces
    awk -v S="$2" '!/^#/ && $1 == S' "$1" | tr -s ' ' | sed 's/^ //'
}

steps_ok() { # steps_ok <trace file>: "<lines> <1 if the step column is 1, 2, 3, ...>"
    awk '!/^#/ { n++; if ($1 != n) bad = 1 } END { print n, bad ? 0 : 1 }' "$1"
}

echo "complex_mul: (3 + 4i)(5 + 2i), registers known by hand"
run cm complex_mul.txt complex_mul_data.byte
R="$WORK/cm/complex_mul_1.regs"
check "13 instructions, finished" "$(sed -n 1p "$R" | sed 's/.* after //')" "13 instructions (finished)"
check "PC after the halt (instruction 12) = 13 * 4 = 52" "$(sed -n 2p "$R")" "PC 52 (0x00000034) = instruction 13"
check "flags from x10 = 6 + 20: all clear" "$(sed -n 3p "$R")" "flags N=0 Z=0 C=0 V=0"
check "x1..x10 = a b c d ac bd ac-bd ad bc ad+bc" "$(xlist "$R")" "x1=3,x2=4,x3=5,x4=2,x5=15,x6=8,x7=7,x8=6,x9=20,x10=26"
check "x10 in hex" "$(xreg "$R" x10)" "26 0x0000001A"
check "no vector register used" "$(grep -c '^vector registers that are not zero: 0$' "$R")" 1
check "no .trace without -T" "$(ls "$WORK/cm" | grep -c '\.trace$')" 0
check "summary line printed" "$(grep -c '^\[regs\] pid 1 (complex_mul): 10 x and 0 vector register(s) not zero -> complex_mul_1.regs$' "$WORK/cm/run.txt")" 1

echo "det3x3: [[6,1,1],[4,2,5],[2,8,7]], det = -146"
run det det3x3.txt det3x3_data.byte
R="$WORK/det/det3x3_1.regs"
check "x12 = a(ei - fh) = 6 * (14 - 40) = -156" "$(xreg "$R" x12)" "-156 0xFFFFFF64"
check "x13 = b(di - fg) = 1 * (28 - 10) = 18" "$(xreg "$R" x13)" "18 0x00000012"
check "x14 = c(dh - eg) = 1 * (32 - 4) = 28" "$(xreg "$R" x14)" "28 0x0000001C"
check "x15 = -156 - 18 + 28 = -146" "$(xreg "$R" x15)" "-146 0xFFFFFF6E"
check "x10, x11 keep the last products dh = 32, eg = 4" "$(xreg "$R" x10)|$(xreg "$R" x11)" "32 0x00000020|4 0x00000004"
check "15 non-zero x registers" "$(sed -n 4p "$R")" "x registers that are not zero: 15"
check "flags of -174 + 28 = -146: N=1" "$(sed -n 3p "$R")" "flags N=1 Z=0 C=0 V=0"
check "PC = 25 * 4 = 100" "$(sed -n 2p "$R")" "PC 100 (0x00000064) = instruction 25"

echo "array_add (N = 16): vector registers after the second (last) iteration"
run aa array_add.txt array_add_data.byte
R="$WORK/aa/array_add_1.regs"
check "v1 = A[8..15]" "$(vreg "$R" v1)" "9 10 11 12 13 14 15 16"
check "v2 = B[8..15]" "$(vreg "$R" v2)" "108 109 110 111 112 113 114 115"
check "v3 = v1 + v2" "$(vreg "$R" v3)" "117 119 121 123 125 127 129 131"
check "3 non-zero vector registers" "$(grep -c '^vector registers that are not zero: 3$' "$R")" 1
check "x registers (x7 = 0 not listed)" "$(xlist "$R")" "x1=16,x2=68,x3=64,x4=132,x5=196,x6=16"
check "flags of x7 = 16 - 16: Z=1" "$(sed -n 3p "$R")" "flags N=0 Z=1 C=0 V=0"

echo "-T trace: one line per executed instruction"
run tr -T complex_mul.txt complex_mul_data.byte sum_array.txt sum_array_data.byte
T="$WORK/tr/complex_mul_1.trace"
check "complex_mul: 13 lines, steps 1..13" "$(steps_ok "$T")" "13 1"
check "step 1: bytes and loaded value" "$(tline "$T" 1)" "1 0 0D 01 00 00 x1 = [0] x1 = 3 (0x00000003)"
check "step 7: subtraction with flags (C = 15 > 8)" "$(tline "$T" 7)" "7 6 02 07 05 06 x7 = x5 - x6 x7 = 7 (0x00000007) NZCV=0010"
check "step 11: store" "$(tline "$T" 11)" "11 10 0E 10 00 07 [16] = x7 [16] = 7"
check "step 13: halt" "$(tline "$T" 13)" "13 12 00 00 00 00 halt end of task"
T="$WORK/tr/sum_array_2.trace"
check "sum_array: 71 lines, steps 1..71" "$(steps_ok "$T")" "71 1"
check "step 4: x4 = size 9" "$(tline "$T" 4)" "4 3 05 04 00 03 x4 = [x3] x4 = 9 (0x00000009)"
check "step 6: BEQ not taken" "$(tline "$T" 6)" "6 5 10 00 00 06 BEQ +6 not taken"
check "step 11: BAL back to the loop" "$(tline "$T" 11)" "11 10 1E 00 00 FA BAL -6 taken -> instruction 4"
check "step 69: BEQ taken to the exit" "$(tline "$T" 69)" "69 5 10 00 00 06 BEQ +6 taken -> instruction 11"
check "step 70: sum stored at 36" "$(tline "$T" 70)" "70 11 06 03 00 02 [x3] = x2 [36] = 45"
check "summary names the trace file" "$(grep -c 'sum_array_2.regs, sum_array_2.trace$' "$WORK/tr/run.txt")" 1

echo "-T with 4 tasks at once: the trace files do not mix"
run tr4 -T complex_mul.txt complex_mul_data.byte sum_array.txt sum_array_data.byte \
           det3x3.txt det3x3_data.byte array_add.txt array_add_data.byte
check "complex_mul 13, sum_array 71, det3x3 25, array_add 31 lines" \
      "$(steps_ok "$WORK/tr4/complex_mul_1.trace")|$(steps_ok "$WORK/tr4/sum_array_2.trace")|$(steps_ok "$WORK/tr4/det3x3_3.trace")|$(steps_ok "$WORK/tr4/array_add_4.trace")" \
      "13 1|71 1|25 1|31 1"
check "array_add step 22: v3 in the second iteration" "$(tline "$WORK/tr4/array_add_4.trace" 22)" \
      "22 10 21 03 01 02 v3 = v1 + v2 v3 = 117 119 121 123 125 127 129 131"
check "array_add step 23: vector store at C[8..15]" "$(tline "$WORK/tr4/array_add_4.trace" 23)" \
      "23 11 26 05 00 03 [x5] = v3 [164..195] = v3"
check "det3x3 .regs still right (x15 = -146)" "$(xreg "$WORK/tr4/det3x3_3.regs" x15)" "-146 0xFFFFFF6E"

echo "a task that stops with an error"
mkdir -p "$WORK/err" && printf '00 00 00 00\n' > "$WORK/err/d.byte"
printf 'x1 = 5\nx2 = 0\nx3 = x1 / x2\n' > "$WORK/err/div0.txt"
(cd "$WORK/err" && "$SIM" -b -T div0.txt d.byte > run.txt 2>&1)
check "trace ends at the failing division" "$(steps_ok "$WORK/err/div0_1.trace")|$(tline "$WORK/err/div0_1.trace" 3)" \
      "3 1|3 2 04 03 01 02 x3 = x1 / x2 error: the task stopped here"
check ".regs written: stopped with an error, only x1 = 5" "$(sed -n 1p "$WORK/err/div0_1.regs" | sed 's/.*(//')|$(xlist "$WORK/err/div0_1.regs")" \
      "stopped with an error)|x1=5"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
