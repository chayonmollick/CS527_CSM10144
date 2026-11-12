#!/usr/bin/env bash
# Lab 2 tests: build ./sim, run every example in a scratch directory, and check the
# results, the byte code (against the handout's listing), the flags, the branch
# conditions and the error handling.
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

# words <file> <byte address> <count>: <count> signed 32-bit words from that address,
# space separated. Lab 2 .byte files hold hex bytes, least significant byte first.
words() {
    awk -v A="$2" -v N="$3" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { for (k = 0; k < N; k++) {
                  a = A + 4 * k
                  v = b[a] + b[a+1] * 256 + b[a+2] * 65536 + b[a+3] * 16777216
                  if (v >= 2147483648) v -= 4294967296
                  printf "%s%d", (k ? " " : ""), v
              }
              print "" }' "$1"
}
word_at() { words "$1" "$2" 1; }

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

# two_digit: hex bytes as two upper-case digits ("F 1 0 0" -> "0F 01 00 00"), lines joined with |
two_digit() {
    awk '{ s = ""
           for (i = 1; i <= NF; i++) { h = toupper($i); if (length(h) == 1) h = "0" h; s = s (i > 1 ? " " : "") h }
           out = out (NR > 1 ? "|" : "") s }
         END { print out }'
}

# The handout's byte code for its sum-of-array example, exactly as printed there.
HANDOUT_LISTING='F 1 0 0
F 2 0 0
F 3 0 0
5 4 0 3
2  F 1 4
10 0 0 6
9 3 3 4
5 6 0 3
1 2 2 6
9 1 1 1
1E 0 0 FA
6 3 0 2'

echo "Handout example: sum of an array"
check "sum_array runs (exit code 0)" "$(run sa sum_array.txt sum_array_data.byte)" 0
check "byte code = the handout's 12-line listing" "$(sed '$d' "$WORK/sa/program.byte" | two_digit)" \
      "$(printf '%s\n' "$HANDOUT_LISTING" | two_digit)"
check "the compiler adds 00 00 00 00 at the end" "$(tail -n 1 "$WORK/sa/program.byte")" "00 00 00 00"
check "BEQ .exit: forward offset +6" "$(sed -n 6p "$WORK/sa/program.byte")" "10 00 00 06"
check "BAL .loopback: backward offset -6 = FA" "$(sed -n 11p "$WORK/sa/program.byte")" "1E 00 00 FA"
check "sum of 1..9 = 45, stored at [x3] = 4N = 36" "$(word_at "$WORK/sa/sum_array_data.out" 36)" 45

echo "Suggested programs"
run sn sum_n.txt sum_n_data.byte > /dev/null
check "sum of N given at run time: 10 - 1 + 20 + 30 + 40 at 4N + 4" "$(word_at "$WORK/sn/sum_n_data.out" 24)" 99
run fir fir.txt fir_data.byte > /dev/null
check "FIR y[0..7] for x = 1..8, weights 1 2 3 4" "$(words "$WORK/fir/fir_data.out" 256 8)" "1 4 10 20 30 40 50 60"
check "FIR leaves N and the input in place" "$(words "$WORK/fir/fir_data.out" 0 9)" "8 1 2 3 4 5 6 7 8"
printf '06 00 00 00\n01 00 00 00\n' > "$WORK/fir/impulse.byte"
printf '00 00 00 00\n%.0s' 1 2 3 4 5 >> "$WORK/fir/impulse.byte"
(cd "$WORK/fir" && "$SIM" fir.txt impulse.byte > run2.txt 2>&1)
check "FIR impulse response = the weights" "$(words "$WORK/fir/impulse.out" 256 6)" "1 2 3 4 0 0"
run max max_array.txt max_array_data.byte > /dev/null
check "max of 7 -3 42 15 -100 41" "$(word_at "$WORK/max/max_array_data.out" 28)" 42
printf '03 00 00 00\nFB FF FF FF\nFE FF FF FF\nF7 FF FF FF\n' > "$WORK/max/neg.byte"
(cd "$WORK/max" && "$SIM" max_array.txt neg.byte > run2.txt 2>&1)
check "max of -5 -2 -9 (all negative)" "$(word_at "$WORK/max/neg.out" 16)" -2
printf '01 00 00 00\nF9 FF FF FF\n' > "$WORK/max/one.byte"
(cd "$WORK/max" && "$SIM" max_array.txt one.byte > run3.txt 2>&1)
check "max of a single element -7" "$(word_at "$WORK/max/one.out" 8)" -7

echo "Byte code of every Lab 2 statement"
mkdir -p "$WORK/enc"
cat > "$WORK/enc/all.txt" <<'EOF'
% every statement form; blank lines, comments and a trailing ';' are ignored

x1 = 7            % 0F D 0 c   xD = c
x2 = x1           % 07 D 0 A   xD = xA
x3 = [x2]         % 05 D 0 A   xD = [xA]
x4 = [12]         % 0D D 0 c   xD = [c]
[x2] = x3         % 06 A 0 S   [xA] = xS
[16] = x4;        % 0E c 0 S   [c] = xS
Read x5, 8        % legacy, same as x5 = [8]
Write x5 20       % legacy, same as [20] = x5
x6 = x1 + x2
x6 = x1 - x2
x6 = x1 * x2
x6 = x1 / x2
x7 = x1 + 0x10
x7 = x1 - 200
x7 = x1 * 3
x7 = x1 / 2
EOF
(cd "$WORK/enc" && "$SIM" -c all.txt > listing.txt 2>&1)
check "statements encode as in the handout's table (operand 1 = 0 for memory and moves)" \
      "$(lines "$WORK/enc/program.byte")" \
      "0F 01 00 07|07 02 00 01|05 03 00 02|0D 04 00 0C|06 02 00 03|0E 10 00 04|0D 05 00 08|0E 14 00 05|01 06 01 02|02 06 01 02|03 06 01 02|04 06 01 02|09 07 01 10|0A 07 01 C8|0B 07 01 03|0C 07 01 02|00 00 00 00"
{ echo ".top"; for c in EQ NE CS CC MI PL VS VC HI LS GE LT GT LE AL; do echo "B$c .top"; done
  echo "B .top"; echo "BHS .top"; echo "BLO .top"; echo "beq .top"; } > "$WORK/enc/br.txt"
(cd "$WORK/enc" && "$SIM" -c br.txt br.byte > /dev/null 2>&1)
check "B<cond> opcodes 10..1E (B = BAL, HS = CS, LO = CC, any case)" \
      "$(cut -d' ' -f1 "$WORK/enc/br.byte" | paste -sd' ' -)" "10 11 12 13 14 15 16 17 18 19 1A 1B 1C 1D 1E 1E 12 13 10 00"
check "offset = label index - branch index (18 back = EE)" "$(sed -n 19p "$WORK/enc/br.byte")" "10 00 00 EE"
printf 'x1 = 3\nx2 = 0\n  .back   %% indented label, comment after it\nx2 = x2 + 1\nx1 = x1 - 1\nBNE .back\n[0] = x2\n' \
       > "$WORK/enc/lbl.txt"
printf '00 00 00 00\n' > "$WORK/enc/lbl_data.byte"
(cd "$WORK/enc" && "$SIM" lbl.txt lbl_data.byte > run.txt 2>&1)
check "label with leading blanks and a comment after it (loop runs 3 times)" "$(word_at "$WORK/enc/lbl_data.out" 0)" 3

echo "Memory access: [xA], [c], legacy Read/Write"
mkdir -p "$WORK/mem"
printf 'x1 = 8\nx2 = [x1]\nx3 = [12]\nx4 = x2 - x3\n[x1] = x4\n[16] = x2\nRead x5, 12\nWrite x5, 20\n' > "$WORK/mem/m.txt"
printf 'x6 = x5\nx7 = x6 * 0x10\n[24] = x7\n' >> "$WORK/mem/m.txt"
printf '00 00 00 00\n00 00 00 00\n64 00 00 00\n32 00 00 00\n' > "$WORK/mem/m_data.byte"
(cd "$WORK/mem" && "$SIM" m.txt m_data.byte > run.txt 2>&1)
check "[8] = 100 - 50, [16] = 100, [20] = 50 (Write), [24] = 50 * 16" "$(words "$WORK/mem/m_data.out" 8 5)" "50 50 100 50 800"
printf 'Read x1, 0\nRead x2, 4\nRead x3, 8\nRead x4, 12\nx5 = x1 + x2\nx6 = x3 + x4\nx7 = x5 + x6\nWrite x7, 16\n' > "$WORK/mem/legacy.txt"
printf '0A 00 00 00\n14 00 00 00\n1E 00 00 00\n28 00 00 00\n00 00 00 00\n' > "$WORK/mem/legacy_data.byte"
(cd "$WORK/mem" && "$SIM" legacy.txt legacy_data.byte > run2.txt 2>&1)
check "Lab 1's add-four-variables program still runs" "$(word_at "$WORK/mem/legacy_data.out" 16)" 100
check "Read x1, 0 -> 0D 01 00 00, Write x7, 16 -> 0E 10 00 07" \
      "$(sed -n 1p "$WORK/mem/program.byte")|$(sed -n 8p "$WORK/mem/program.byte")" "0D 01 00 00|0E 10 00 07"

echo "Flags Z N C V (only add and subtract change them)"
# flags <statements>: run them, read the flags back with BNE/BPL/BCC/BVC; prints "Z N C V"
flags() {
    local d="$WORK/flags"
    mkdir -p "$d" && printf '00 00 00 00\n' > "$d/d.byte"
    { printf '%b\n' "$1"
      printf 'x21 = 0\nx22 = 0\nx23 = 0\nx24 = 0\n'
      printf 'BNE .z\nx21 = 1\n.z\nBPL .n\nx22 = 1\n.n\nBCC .c\nx23 = 1\n.c\nBVC .v\nx24 = 1\n.v\n'
      printf '[0] = x21\n[4] = x22\n[8] = x23\n[12] = x24\n'; } > "$d/f.txt"
    (cd "$d" && "$SIM" f.txt d.byte > run.txt 2>&1)
    words "$d/d.out" 0 4
}
MIN='x1 = 128\nx1 = x1 * x1\nx1 = x1 * x1\nx1 = x1 * 8'     # x1 = 0x80000000, the most negative number
check "add 0x7FFFFFFF + 1: N and V (signed overflow)" "$(flags "$MIN\nx2 = x1 - 1\nx3 = x2 + 1")" "0 1 0 1"
check "add -1 + 1 = 0: Z and C (carry out)" "$(flags 'x1 = 0\nx1 = x1 - 1\nx2 = 1\nx3 = x1 + x2')" "1 0 1 0"
check "add 2 + 3: no flag" "$(flags 'x1 = 2\nx2 = x1 + 3')" "0 0 0 0"
check "sub 5 - 5 = 0: Z only (C needs operand 1 > operand 2)" "$(flags 'x1 = 5\nx2 = x1 - 5')" "1 0 0 0"
check "sub 5 - 3: C" "$(flags 'x1 = 5\nx2 = x1 - 3')" "0 0 1 0"
check "sub 3 - 5 = -2: N" "$(flags 'x1 = 3\nx2 = 5\nx3 = x1 - x2')" "0 1 0 0"
check "sub 0x80000000 - 1: C and V (signed overflow)" "$(flags "$MIN\nx2 = x1 - 1")" "0 0 1 1"
check "mul, div, moves, loads and stores leave the flags alone" \
      "$(flags 'x1 = 5\nx2 = x1 - 5\nx3 = x1 * 7\nx4 = x1 / 1\nx5 = x1\n[0] = x1\nx6 = [0]')" "1 0 0 0"

echo "Branch conditions after a subtraction (taken = 1)"
# conds <statements>: for every condition, is B<cond> taken after them? -> "EQ=0 NE=1 ..."
conds() {
    local d="$WORK/cond" c out=""
    mkdir -p "$d" && printf '00 00 00 00\n' > "$d/d.byte"
    for c in EQ NE CS CC MI PL VS VC HI LS GE LT GT LE AL; do
        printf '%b\nx9 = 1\nB%s .yes\nx9 = 0\n.yes\n[0] = x9\n' "$1" "$c" > "$d/c.txt"
        (cd "$d" && "$SIM" c.txt d.byte > run.txt 2>&1)
        out="$out $c=$(word_at "$d/d.out" 0)"
    done
    echo "${out# }"
}
check "3 - 5: signed and unsigned less" "$(conds 'x1 = 3\nx2 = x1 - 5')" \
      "EQ=0 NE=1 CS=0 CC=1 MI=1 PL=0 VS=0 VC=1 HI=0 LS=1 GE=0 LT=1 GT=0 LE=1 AL=1"
check "5 - 5: equal" "$(conds 'x1 = 5\nx2 = x1 - 5')" \
      "EQ=1 NE=0 CS=0 CC=1 MI=0 PL=1 VS=0 VC=1 HI=0 LS=1 GE=1 LT=0 GT=0 LE=1 AL=1"
check "5 - 3: signed and unsigned greater" "$(conds 'x1 = 5\nx2 = x1 - 3')" \
      "EQ=0 NE=1 CS=1 CC=0 MI=0 PL=1 VS=0 VC=1 HI=1 LS=0 GE=1 LT=0 GT=1 LE=0 AL=1"
check "0x80000000 - 1: signed less (N != V), unsigned higher" "$(conds "$MIN\nx2 = x1 - 1")" \
      "EQ=0 NE=1 CS=1 CC=0 MI=0 PL=1 VS=1 VC=0 HI=1 LS=0 GE=0 LT=1 GT=0 LE=1 AL=1"

echo "Files and options"
check "default output name: <data file>.out" "$(ls "$WORK/sa" | grep -c '^sum_array_data\.out$')" 1
run named sum_n.txt sum_n_data.byte result.byte > /dev/null
check "a third argument names the output file" "$(word_at "$WORK/named/result.byte" 24)" 99
mkdir -p "$WORK/pre" && cp "$WORK/sa/program.byte" "$WORK/pre/sum.byte" && cp "$EX/sum_array_data.byte" "$WORK/pre/"
(cd "$WORK/pre" && "$SIM" sum.byte sum_array_data.byte > run.txt 2>&1)
check "an already compiled .byte program runs as is" "$(word_at "$WORK/pre/sum_array_data.out" 36)" 45
check "-c prints the listing" \
      "$(cd "$WORK/sa" && "$SIM" -c sum_array.txt listing.byte | grep -cF '10: 1E 00 00 FA   | BAL .loopback')" 1
check "-c writes the same byte code" "$(cmp -s "$WORK/sa/listing.byte" "$WORK/sa/program.byte" && echo same)" same
mkdir -p "$WORK/size"
awk 'BEGIN { print ".top"; for (i = 0; i < 62; i++) print "x1 = x1 + 1"; print "BNE .top" }' > "$WORK/size/p63.txt"
awk 'BEGIN { for (i = 0; i < 64; i++) print "x1 = 1" }' > "$WORK/size/p64.txt"
check "63 statements + the end fill Instruction[256]" "$(cd "$WORK/size" && "$SIM" -c p63.txt > /dev/null 2>&1; echo $?)" 0
check "64 statements do not fit" "$(cd "$WORK/size" && "$SIM" -c p64.txt 2>&1 | grep -c '64 instructions do not fit')" 1

echo "Compile errors: file:line message, exit code 1, no byte code written"
mkdir -p "$WORK/err"
compile_error() {   # compile_error <name> <program text> <expected message>
    printf '%b' "$2" > "$WORK/err/e.txt"
    rm -f "$WORK/err/e.byte"
    (cd "$WORK/err" && "$SIM" -c e.txt e.byte > out.txt 2>&1)
    local rc=$?
    check "$1" "$rc $(grep -cF "$3" "$WORK/err/out.txt") $([ -e "$WORK/err/e.byte" ] && echo written || echo none)" "1 1 none"
}
compile_error "unknown label" 'x1 = 1\nBNE .nowhere\n' "e.txt:2: unknown label .nowhere"
compile_error "label defined twice" '.a\nx1 = 1\n.a\n' "e.txt:3: label .a defined twice"
compile_error "label with a special character" '.lo-op\n' "e.txt:1: bad label '.lo-op'"
compile_error "label not alone on its line" '.loop x1 = 1\n' "e.txt:1: bad label '.loop x1 = 1'"
compile_error "branch without a label" 'BEQ\n' "e.txt:1: usage: B<cond> .label"
compile_error "register x256" '% comment\nx256 = 1\n' "e.txt:2: register x256 does not exist"
compile_error "constant 256" 'x1 = x2 + 256\n' "e.txt:1: constant 256 is out of range 0..255"
compile_error "constant as the first operand" 'x1 = 5 + x2\n' "e.txt:1: the first operand must be a register"
compile_error "memory to memory" '[4] = [8]\n' "e.txt:1: only a register can be written to memory"
compile_error "Write with a register address" 'Write x1, x2\n' "e.txt:1: usage: Write x<n>, <address 0..255>"
compile_error "statement without '='" 'x1 x2\n' "e.txt:1: statement not recognised"

echo "Run-time errors: message, exit code 1, the run stops"
runtime_error() {   # runtime_error <name> <program text> <expected message>
    printf '%b' "$2" > "$WORK/err/r.txt"
    printf '00 00 00 00\n' > "$WORK/err/r_data.byte"
    (cd "$WORK/err" && "$SIM" r.txt r_data.byte > out.txt 2>&1)
    local rc=$?
    check "$1" "$rc $(grep -cF "$3" "$WORK/err/out.txt")" "1 1"
}
runtime_error "division by zero" 'x1 = 5\nx3 = x1 / x2\n[0] = x1\n' "error at instruction 1: division by zero (x2 is 0)"
check "nothing runs after the error" "$(word_at "$WORK/err/r_data.out" 0)" 0
runtime_error "division by the constant 0" 'x1 = 5\nx3 = x1 / 0\n' "division by zero (constant 0)"
runtime_error "address past the end of Data[4096]" 'x1 = 128\nx1 = x1 * 32\nx2 = [x1]\n' \
              "data address 4096 is outside Data[4096]"
runtime_error "negative address" 'x1 = 0\nx1 = x1 - 4\n[x1] = x1\n' "data address -4 is outside Data[4096]"
runtime_error "endless loop is stopped" '.forever\nB .forever\n' "endless loop?"
printf '1E 00 00 80\n' > "$WORK/err/far.byte"
check "a branch out of the program" \
      "$(cd "$WORK/err" && "$SIM" far.byte r_data.byte 2>&1 | grep -c 'PC -512 is outside the instruction memory')" 1
printf '08 00 00 00\n' > "$WORK/err/op8.byte"
check "unknown opcode (08 is print, Lab 4)" "$(cd "$WORK/err" && "$SIM" op8.byte r_data.byte 2>&1 | grep -c 'unknown opcode 0x08')" 1
printf '0A 00 00 GG\n' > "$WORK/err/bad_data.byte"
check "a data file with a non-hex byte is rejected" \
      "$(cd "$WORK/err" && "$SIM" r.txt bad_data.byte 2>&1 | grep -c "bad_data.byte:1: 'GG' is not a hex byte")" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
