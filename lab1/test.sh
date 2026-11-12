#!/usr/bin/env bash
# Lab 1 tests: build ./sim, run every example in a scratch directory, and check the
# results, the byte code in program.byte and the error handling.
#   bash test.sh   (or: make test)        SIM=/path/to/sim bash test.sh  tests another build
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${SIM:-}" ]; then
    (cd "$HERE" && make -s sim > /dev/null) || { echo "build failed"; exit 1; }
    SIM="$HERE/sim"
fi
SIM="$(cd "$(dirname "$SIM")" && pwd)/$(basename "$SIM")"     # absolute: the tests cd around
EX="$HERE/examples"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

# word_at <file> <byte address>: the signed 32-bit word stored there. Lab 1 .byte
# files hold decimal bytes, four per line, least significant byte first.
word_at() {
    awk -v A="$2" '
        { for (i = 1; i <= NF; i++) b[n++] = $i + 0 }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296
              printf "%d\n", v }' "$1"
}

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

# run <dir> <sim arguments...>: run the simulator inside $WORK/<dir>, a fresh copy of
# examples/, and print its exit code (its output goes to <dir>/run.txt)
run() {
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$dir"/ && (cd "$dir" && "$SIM" "$@" > run.txt 2>&1)
    echo $?
}

lines() { paste -sd'|' - < "$1"; }    # a file's lines joined with |

echo "Handout example: add four variables (Read/Write, +)"
check "sum4 runs (exit code 0)" "$(run sum4 sum4.txt sum4_data.byte)" 0
check "sum4 program.byte = the handout's listing + 0 0 0 0" "$(lines "$WORK/sum4/program.byte")" \
      "5 1 0 0|5 2 4 0|5 3 8 0|5 4 12 0|1 5 1 2|1 6 3 4|1 7 5 6|6 7 16 0|0 0 0 0"
check "sum4 [16] = 10 + 20 + 30 + 40" "$(word_at "$WORK/sum4/sum4_data.out" 16)" 100
check "sum4_data.out = the data memory, same format" "$(lines "$WORK/sum4/sum4_data.out")" \
      "10 0 0 0|20 0 0 0|30 0 0 0|40 0 0 0|100 0 0 0"

echo "Suggested programs"
run sumn sum_n_fixed.txt sum_n_fixed_data.byte > /dev/null
check "sum of N = 6 numbers: 5 + 12 - 3 + 100 + 7 + 1000" "$(word_at "$WORK/sumn/sum_n_fixed_data.out" 24)" 1121
run cmul complex_mul.txt complex_mul_data.byte > /dev/null
check "complex (1+2i)(3+4i): real part" "$(word_at "$WORK/cmul/complex_mul_data.out" 16)" -5
check "complex (1+2i)(3+4i): imaginary part" "$(word_at "$WORK/cmul/complex_mul_data.out" 20)" 10
run det det3x3.txt det3x3_data.byte > /dev/null
check "determinant of [[6,1,1],[4,2,5],[2,8,7]]" "$(word_at "$WORK/det/det3x3_data.out" 36)" -146
check "-146 is stored little-endian as 110 255 255 255" "$(sed -n 10p "$WORK/det/det3x3_data.out")" "110 255 255 255"

echo "32-bit arithmetic"
mkdir -p "$WORK/ops"
printf 'x1 = 7\nx2 = 2\nx3 = x2 - x1\nx4 = x3 / x2\nx5 = x1 * x2\nx6 = 255\nx7 = x6 * x6\nx7 = x7 * x7\n' > "$WORK/ops/ops.txt"
printf 'x8 = x5 / x1\nWrite x3, 0\nWrite x4, 4\nWrite x5, 8\nWrite x7 12\nWrite x8 16\nRead x9 0\nWrite x9, 20\n' >> "$WORK/ops/ops.txt"
printf '0 0 0 0\n' > "$WORK/ops/d.byte"
(cd "$WORK/ops" && "$SIM" ops.txt d.byte > run.txt 2>&1)
check "2 - 7 = -5" "$(word_at "$WORK/ops/d.out" 0)" -5
check "-5 / 2 = -2 (rounds toward zero)" "$(word_at "$WORK/ops/d.out" 4)" -2
check "7 * 2 = 14" "$(word_at "$WORK/ops/d.out" 8)" 14
check "65025 * 65025 wraps around to 32 bits" "$(word_at "$WORK/ops/d.out" 12)" -66716671
check "14 / 7 = 2" "$(word_at "$WORK/ops/d.out" 16)" 2
check "Read/Write without the comma; read back -5" "$(word_at "$WORK/ops/d.out" 20)" -5
check "writes past the data file grow the output (6 words)" "$(wc -l < "$WORK/ops/d.out" | tr -d ' ')" 6

echo "Files and options"
check "default output name: <data file>.out" "$(ls "$WORK/sum4" | grep -c '^sum4_data\.out$')" 1
run named sum4.txt sum4_data.byte result.byte > /dev/null
check "a third argument names the output file" "$(word_at "$WORK/named/result.byte" 16)" 100
mkdir -p "$WORK/pre" && cp "$WORK/sum4/program.byte" "$WORK/pre/sum.byte" && cp "$EX/sum4_data.byte" "$WORK/pre/"
(cd "$WORK/pre" && "$SIM" sum.byte sum4_data.byte > run.txt 2>&1)
check "an already compiled .byte program runs as is" "$(word_at "$WORK/pre/sum4_data.out" 16)" 100
check "-c prints the listing" \
      "$(cd "$WORK/sum4" && "$SIM" -c sum4.txt listing.byte | grep -cF '3:   5   4  12   0   | Read x4, 12')" 1
check "-c writes the same byte code" "$(cmp -s "$WORK/sum4/listing.byte" "$WORK/sum4/program.byte" && echo same)" same
printf 'READ X1, 0\n\n   write x1 4\n' > "$WORK/sum4/case.txt"
(cd "$WORK/sum4" && "$SIM" -c case.txt case.byte > /dev/null 2>&1)
check "Read/Write/x in any case, blank lines and indentation" "$(lines "$WORK/sum4/case.byte")" "5 1 0 0|6 1 4 0|0 0 0 0"
mkdir -p "$WORK/max"
awk 'BEGIN { for (i = 0; i < 63; i++) print "x1 = 1" }' > "$WORK/max/p63.txt"
awk 'BEGIN { for (i = 0; i < 64; i++) print "x1 = 1" }' > "$WORK/max/p64.txt"
check "63 statements + the end fill Instruction[256]" "$(cd "$WORK/max" && "$SIM" -c p63.txt > /dev/null 2>&1; echo $?)" 0
check "64 statements do not fit" \
      "$(cd "$WORK/max" && "$SIM" -c p64.txt 2>&1 | grep -c '64 instructions do not fit')" 1

echo "Compile errors: file:line message, exit code 1, no byte code written"
mkdir -p "$WORK/err"
compile_error() {   # compile_error <name> <program text> <expected message>
    printf '%b' "$2" > "$WORK/err/e.txt"
    rm -f "$WORK/err/e.byte"
    (cd "$WORK/err" && "$SIM" -c e.txt e.byte > out.txt 2>&1)
    local rc=$?
    check "$1" "$rc $(grep -cF "$3" "$WORK/err/out.txt") $([ -e "$WORK/err/e.byte" ] && echo written || echo none)" "1 1 none"
}
compile_error "constant in arithmetic (Lab 2)" 'x1 = 5\nx2 = x1 + 10\n' "e.txt:2: both operands must be registers"
compile_error "square brackets (Lab 2)" 'x1 = [4]\n' "e.txt:1: square brackets are not part of Lab 1"
compile_error "label (Lab 2)" 'x1 = 1\n.loop\n' "e.txt:2: labels are not part of Lab 1"
compile_error "branch (Lab 2)" 'BEQ .loop\n' "e.txt:1: statement not recognised"
compile_error "comment (Lab 2)" 'x1 = 1 % one\n' "e.txt:1: comments (%) are not part of the Lab 1 language"
compile_error "register copy xD = xA (Lab 2)" 'x1 = x2\n' "e.txt:1: data movement assigns a constant"
compile_error "register x256" '\nx256 = 1\n' "e.txt:2: register x256 does not exist"
compile_error "constant 256" 'x1 = 256\n' "e.txt:1: constant 256 is out of range 0..255"
compile_error "Read with a register address" 'Read x1, x2\n' "e.txt:1: usage: Read x<n>, <address 0..255>"
compile_error "unknown operator" 'x1 = x2 ^ x3\n' "e.txt:1: unknown operator '^'"

echo "Run-time errors: message, exit code 1, the run stops"
runtime_error() {   # runtime_error <name> <program text> <expected message>
    printf '%b' "$2" > "$WORK/err/r.txt"
    printf '0 0 0 0\n' > "$WORK/err/r_data.byte"
    (cd "$WORK/err" && "$SIM" r.txt r_data.byte > out.txt 2>&1)
    local rc=$?
    check "$1" "$rc $(grep -cF "$3" "$WORK/err/out.txt")" "1 1"
}
runtime_error "division by zero" 'x1 = 5\nx3 = x1 / x2\nWrite x1, 0\n' "error at instruction 1: division by zero (x2 is 0)"
check "nothing runs after the error" "$(word_at "$WORK/err/r_data.out" 0)" 0
runtime_error "a word past the end of Data[256]" 'Read x1, 253\n' "data address 253 is outside Data[256]"
printf '9 0 0 0\n' > "$WORK/err/bad_op.byte"
check "unknown opcode in a .byte program" \
      "$(cd "$WORK/err" && "$SIM" bad_op.byte r_data.byte 2>&1 | grep -c 'unknown opcode 9')" 1
awk 'BEGIN { for (i = 0; i < 64; i++) print "7 1 1 0" }' > "$WORK/err/no_end.byte"
check "a full .byte program without an end stops at the end of Instruction[256]" \
      "$(cd "$WORK/err" && "$SIM" no_end.byte r_data.byte 2>&1 | grep -c 'PC 256 is outside the instruction memory')" 1
printf '1 2 3 300\n' > "$WORK/err/bad_data.byte"
check "a data file with a non-byte is rejected" \
      "$(cd "$WORK/err" && "$SIM" r.txt bad_data.byte 2>&1 | grep -c "bad_data.byte:1: '300' is not a decimal byte")" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
