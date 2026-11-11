#!/usr/bin/env bash
# Regression tests for the simulator: run every example and check the results.
#   cd minicomputer/simulator && make test        (or: ../tests/run_tests.sh from anywhere)
#   SIM=$PWD/sim ../../tests/run_tests.sh          test an extension's build against the same checks
# Each test runs in a fresh scratch directory so output files never mix.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
SIM="${SIM:-$HERE/../simulator/sim}"   # SIM=/path/to/other/sim tests another build (e.g. an extension)
EX="$HERE/../examples"
[ -x "$SIM" ] || { echo "build first: (cd $HERE/../simulator && make)"; exit 1; }
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

run() {     # run <test dir> <sim arguments...>   (runs in batch mode)
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$dir"/ && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

echo "Lab 1: legacy Read/Write, arithmetic"
check "lab1_sum4 exit code" "$(run l1 lab1_sum4.txt lab1_sum4_data.byte)" 0
check "lab1_sum4 [16] = 10+20+30+40" "$(word_at "$WORK/l1/lab1_sum4_data_1.out" 16)" 100
run l1b complex_mul.txt complex_mul_data.byte > /dev/null
check "complex_mul real (3+4i)(5+2i)" "$(word_at "$WORK/l1b/complex_mul_data_1.out" 16)" 7
check "complex_mul imaginary" "$(word_at "$WORK/l1b/complex_mul_data_1.out" 20)" 26
run l1c det3x3.txt det3x3_data.byte > /dev/null
check "det3x3 of [[6,1,1],[4,2,5],[2,8,7]]" "$(word_at "$WORK/l1c/det3x3_data_1.out" 36)" -146

echo "Lab 2: labels, branches, flags, [x] memory"
run l2 sum_array.txt sum_array_data.byte > /dev/null
check "sum_array (handout program) [36]" "$(word_at "$WORK/l2/sum_array_data_1.out" 36)" 45
expected_code="0F 01 00 00|0F 02 00 00|0F 03 00 00|05 04 00 03|02 0F 01 04|10 00 00 06|09 03 03 04|05 06 00 03|01 02 02 06|09 01 01 01|1E 00 00 FA|06 03 00 02|00 00 00 00"
check "sum_array byte code = handout listing" "$(paste -sd'|' "$WORK/l2/program.byte")" "$expected_code"
run l2b sum_n.txt sum_n_data.byte > /dev/null
check "sum_n 10 + (-1) + 20 + 30 + 40 at 4N+4" "$(word_at "$WORK/l2b/sum_n_data_1.out" 24)" 99

echo "Lab 3: vectors"
run l3 array_add.txt array_add_data.byte > /dev/null
check "array_add C[0] = 1 + 100" "$(word_at "$WORK/l3/array_add_data_1.out" 132)" 101
check "array_add C[15] = 16 + 115" "$(word_at "$WORK/l3/array_add_data_1.out" 192)" 131
run l3b fir.txt fir_data.byte > /dev/null
check "fir out[0] = 1*1+2*2+..+8*8" "$(word_at "$WORK/l3b/fir_data_1.out" 256)" 204
check "fir out[15] = 36*15 + 204" "$(word_at "$WORK/l3b/fir_data_1.out" 316)" 744

echo "Lab 4: multiprocessing, print, waiting queue"
run l4 counter.txt counter_data.byte counter_by100.txt counter_data.byte counter.txt counter_data.byte \
       counter_by100.txt counter_data.byte counter.txt counter_data.byte > /dev/null
check "5 tasks, 4 processors: pid 5 waited" "$(grep -c 'pid 5 is waiting' "$WORK/l4/run.txt")" 1
check "counter final value (pid 5)" "$(word_at "$WORK/l4/counter_data_5.out" 0)" 10
check "counter_by100 final value (pid 4)" "$(word_at "$WORK/l4/counter_data_4.out" 0)" 1000
check "50 print lines in log.txt" "$(wc -l < "$WORK/l4/log.txt" | tr -d ' ')" 50
check "round robin (assumes NP = 4): each slice prints twice, pids 1-4 in turn" "$(head -8 "$WORK/l4/log.txt" | awk '{print $3}' | paste -sd, -)" "1,1,2,2,3,3,4,4"

echo "Lab 5: paging"
run l5 pages.txt pages_data.byte > /dev/null
check "pages: sum of three demand-mapped pages" "$(word_at "$WORK/l5/pages_data_1.out" 0)" 21
check "pages: 3 page faults reported" "$(grep -c '3 page fault' "$WORK/l5/run.txt")" 1
check "pages: value written at 3825" "$(word_at "$WORK/l5/pages_data_1.out" 3825)" 7

echo "Lab 6: frequency report and page tables in memory"
run l6 sum_array_print.txt sum_array_data.byte > /dev/null
check "freq counts match the handout example" "$(cut -f1 "$WORK/l6/sum_array_print_1.freq" | paste -sd, -)" "1,1,1,1,.loopback,10,10,9,9,9,9,9,9,.exit,1"
run l6b sum_array_print.txt sum_array_data.byte sum_array_print.txt sum_array_data.byte \
        array_add.txt array_add_data.byte fir.txt fir_data.byte > /dev/null
check "4 tasks at once: separate .freq files" "$(ls "$WORK/l6b" | grep -c '\.freq$')" 4
check "4 tasks at once: fir still correct" "$(word_at "$WORK/l6b/fir_data_4.out" 256)" 204
check "4 tasks at once: array_add still correct" "$(word_at "$WORK/l6b/array_add_data_3.out" 192)" 131

echo "Running a precompiled .byte program"
mkdir -p "$WORK/byte" && cp "$WORK/l2/program.byte" "$WORK/byte/sum.byte" && cp "$EX/sum_array_data.byte" "$WORK/byte/"
(cd "$WORK/byte" && "$SIM" -b sum.byte sum_array_data.byte > run.txt 2>&1)
check "sum.byte gives the same result" "$(word_at "$WORK/byte/sum_array_data_1.out" 36)" 45
check "freq without source uses disassembly" "$(sed -n 6p "$WORK/byte/sum_1.freq")" "$(printf '10\tBEQ +6')"

echo "Shell"
mkdir -p "$WORK/sh" && cp "$EX"/* "$WORK/sh"/
(cd "$WORK/sh" && printf 'counter.txt counter_data.byte\nbad command here\nexit\n' | "$SIM" > run.txt 2>&1)
check "task started from the shell finished" "$(word_at "$WORK/sh/counter_data_1.out" 0)" 10
check "shell rejects a bad command" "$(grep -c 'usage' "$WORK/sh/run.txt")" 1

echo "Errors"
mkdir -p "$WORK/err" && cd "$WORK/err" && printf '00 00 00 00\n' > d.byte
printf 'x1 = x300 + 1\n' > badreg.txt
"$SIM" -b badreg.txt d.byte > out.txt 2>&1; rc=$?
check "compile error: bad register (exit code 1)" "$rc" 1
check "compile error names file and line" "$(grep -c 'badreg.txt:1: register x300 does not exist' out.txt)" 1
printf 'x1 = 5\nx2 = 0\nx3 = x1 / x2\n' > div0.txt
"$SIM" -b div0.txt d.byte > out.txt 2>&1; rc=$?
check "runtime error: division by zero stops the task" "$(grep -c 'division by zero' out.txt),$rc" "1,1"
printf 'x1 = 255\nx1 = x1 * 20\nx2 = [x1]\n' > far.txt
"$SIM" -b far.txt d.byte > out.txt 2>&1
check "runtime error: address outside the data space" "$(grep -c 'outside the logical data space' out.txt)" 1
check "BNE to an unknown label is a compile error" "$(printf 'BNE .nowhere\n' > nolabel.txt; "$SIM" -b nolabel.txt d.byte 2>&1 | grep -c 'unknown label .nowhere')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
