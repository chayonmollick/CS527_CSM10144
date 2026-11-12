#!/usr/bin/env bash
# Lab 4 tests: build ./sim, run the examples in a fresh temporary directory, check the results.
#   ./test.sh        (or: make test)        takes a few seconds: the shell tests wait for input
# The log-order checks assume NP = 4 and TIME_SLICE = 10 (config.h).
# Prints one line per check, then "N passed, M failed"; exit code 0 = all passed.
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

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

fresh() {   # fresh <test dir>: a new directory holding a copy of examples/
    mkdir -p "$WORK/$1" && cp "$EX"/* "$WORK/$1"/
}

run() {     # run <test dir> <sim arguments...>: batch run in a fresh copy of examples/, print the exit code
    local name="$1"; shift
    fresh "$name" && (cd "$WORK/$name" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

line_of() { # line_of <file> <text>: number of the first line containing text (0 = none)
    awk -v T="$2" 'index($0, T) { print NR; found = 1; exit } END { if (!found) print 0 }' "$1"
}

echo "Build"
build_out="$(make -s -B --no-print-directory -C "$HERE" sim 2>&1)"
check "make builds ./sim (-std=c++17 -O2 -Wall -Wextra)" "$?" 0
check "no compiler warnings" "$(printf '%s\n' "$build_out" | grep -ci warning)" 0
[ -x "$SIM" ] || { echo "no ./sim to test"; exit 1; }

echo "print: byte code and log line"
(cd "$WORK" && printf 'x5 = 255\nx5 = x5 * 255\nprint x5\nx7 = x1 - 5\nprint x7\n' > p.txt \
    && printf '00 00 00 00\n' > p_data.byte && "$SIM" -c p.txt p.byte > /dev/null && "$SIM" -b p.txt p_data.byte > /dev/null)
check "print x5 compiles to 08 00 00 05" "$(sed -n 3p "$WORK/p.byte")" "08 00 00 05"
check "log line: Process id: <pid>   x<R> : <hex>" "$(head -1 "$WORK/log.txt")" "Process id: 1   x5 : 0000FE01"
check "a negative value prints as 32-bit hex" "$(sed -n 2p "$WORK/log.txt")" "Process id: 1   x7 : FFFFFFFB"
check "the handout's Lab 4 sum program: byte code with print" "$(cd "$WORK" && "$SIM" -c "$EX/sum_array.txt" sum.byte > /dev/null; paste -sd'|' sum.byte)" \
      "0F 01 00 00|0F 02 00 00|0F 03 00 00|05 04 00 03|02 0F 01 04|10 00 00 07|09 03 03 04|05 06 00 03|01 02 02 06|09 01 01 01|08 00 00 01|1E 00 00 F9|06 03 00 02|00 00 00 00"

echo "Multiprocessing: 5 tasks on NP = 4 processors (waiting queue, round robin)"
check "5 counters run to the end (exit code 0)" "$(run rr counter.txt counter_data.byte counter_by100.txt counter_data.byte \
      counter.txt counter_data.byte counter_by100.txt counter_data.byte counter.txt counter_data.byte)" 0
RR="$WORK/rr"
check "loader: pids 1-4 get processors 0-3 (pid -> processor)" "$(grep 'loaded on processor' "$RR/run.txt" | head -4 | awk '{print $3 "->" $NF}' | paste -sd, -)" "1->0,2->1,3->2,4->3"
check "pid 5 waits: all processors busy" "$(grep -c 'pid 5 is waiting' "$RR/run.txt")" 1
check "pid 5 starts on processor 0 only after pid 1 left it" \
      "$([ "$(line_of "$RR/run.txt" 'pid 1 (counter) finished on processor 0')" -lt "$(line_of "$RR/run.txt" 'pid 5 (counter) loaded on processor 0')" ] && echo yes)" yes
check "each task has its own Data[]: counter = 10, counter_by100 = 1000" \
      "$(word_at "$RR/counter_data_1.out" 0),$(word_at "$RR/counter_data_2.out" 0),$(word_at "$RR/counter_data_5.out" 0)" "10,1000,10"
check "the input counter_data.byte is not changed" "$(word_at "$RR/counter_data.byte" 0)" 0
check "50 print lines in log.txt" "$(wc -l < "$RR/log.txt" | tr -d ' ')" 50
check "first round: 10 instructions each = 2 prints, pids 1-4 in turn" "$(head -8 "$RR/log.txt" | awk '{print $3}' | paste -sd, -)" "1,1,2,2,3,3,4,4"
check "whole log order (round robin; pid 5 only after a processor is free)" "$(awk '{print $3}' "$RR/log.txt" | paste -sd, -)" \
      "1,1,2,2,3,3,4,4,1,1,1,2,2,3,3,3,4,4,1,1,2,2,3,3,4,4,1,1,1,2,2,3,3,3,4,4,2,2,4,4,5,5,5,5,5,5,5,5,5,5"
check "log.txt is appended to, not overwritten" "$(cd "$RR" && "$SIM" -b counter.txt counter_data.byte > /dev/null; wc -l < log.txt | tr -d ' ')" 60

echo "Lab 3 programs running at the same time"
check "array_add, fir (N = 24), sum_array, fir (N = 64) together" "$(run four array_add.txt array_add_data.byte fir.txt fir_data.byte \
      sum_array.txt sum_array_data.byte fir.txt fir64_data.byte)" 0
F="$WORK/four"
check "one output file per task: <data>_<pid>.out" "$(ls "$F" | grep -c '_[1-4]\.out$')" 4
check "array_add: C = A + B" "$(words "$F/array_add_data_1.out" 132 16)" "$(expected array_add "$EX/array_add_data.byte")"
check "fir N = 24: outputs match the pseudo code" "$(words "$F/fir_data_2.out" 256 16)" "$(expected fir "$EX/fir_data.byte")"
check "fir N = 64: outputs match the pseudo code" "$(words "$F/fir64_data_4.out" 256 56)" "$(expected fir "$EX/fir64_data.byte")"
check "sum_array: sum 45 at 36" "$(word_at "$F/sum_array_data_3.out" 36)" 45
check "sum_array printed the index 1..9 as pid 3" "$(awk '{print $3 ":" $6}' "$F/log.txt" | paste -sd, -)" \
      "3:00000001,3:00000002,3:00000003,3:00000004,3:00000005,3:00000006,3:00000007,3:00000008,3:00000009"
mkdir -p "$WORK/byte" && cp "$F/program.byte" "$WORK/byte/fir64.byte" && cp "$EX/fir64_data.byte" "$WORK/byte/"
(cd "$WORK/byte" && "$SIM" -b fir64.byte fir64_data.byte > run.txt 2>&1)
check "a precompiled .byte program runs as a task" "$(words "$WORK/byte/fir64_data_1.out" 256 56)" "$(expected fir "$EX/fir64_data.byte")"

echo "Shell (non-blocking)"
fresh sh1 && (cd "$WORK/sh1" && printf 'counter.txt counter_data.byte\nbad command here\nexit\n' | "$SIM" > run.txt 2>&1)
check "a task typed at the shell runs" "$(word_at "$WORK/sh1/counter_data_1.out" 0)" 10
check "a bad line gets the usage message" "$(grep -c 'usage' "$WORK/sh1/run.txt")" 1
fresh sh2 && (cd "$WORK/sh2" && (sleep 1; echo "counter.txt counter_data.byte"; echo exit) | "$SIM" counter_by100.txt counter_data.byte > run.txt 2>&1)
check "tasks run while the shell waits for input" \
      "$([ "$(line_of "$WORK/sh2/run.txt" 'pid 1 (counter_by100) finished')" -lt "$(line_of "$WORK/sh2/run.txt" 'pid 2 created')" ] && echo yes)" yes
check "the task typed later also runs" "$(word_at "$WORK/sh2/counter_data_1.out" 0),$(word_at "$WORK/sh2/counter_data_2.out" 0)" "1000,10"
fresh sh3 && (cd "$WORK/sh3" && (printf 'counter.t'; sleep 1; printf 'xt counter_data.byte\nexit\n') | "$SIM" > run.txt 2>&1)
check "a line is run only when Enter arrives" "$(grep -c 'usage' "$WORK/sh3/run.txt"),$(word_at "$WORK/sh3/counter_data_1.out" 0)" "0,10"
fresh sh4 && (cd "$WORK/sh4" && printf 'exit\ncounter.txt counter_data.byte\n' | "$SIM" > run.txt 2>&1)
check "after exit no new task is taken" "$(grep -c 'created' "$WORK/sh4/run.txt")" 0
fresh sh5 && (cd "$WORK/sh5" && printf 'counter.txt counter_data.byte\n' | "$SIM" > run.txt 2>&1); rc=$?
check "end of input acts like exit" "$(grep -c 'end of input' "$WORK/sh5/run.txt"),$rc" "1,0"

echo "Errors"
mkdir -p "$WORK/err" && cd "$WORK/err" && printf '00 00 00 00\n' > d.byte && cp "$EX/counter.txt" .
printf 'x1 = x300 + 1\n' > badreg.txt
"$SIM" -b badreg.txt d.byte counter.txt d.byte > out.txt 2>&1; rc=$?
check "compile error: named, task not created, exit code 1" "$(grep -c 'badreg.txt:1: register x300 does not exist' out.txt),$(grep -c 'pid 1 (counter) finished' out.txt),$rc" "1,1,1"
printf 'x1 = 5\nx2 = 0\nx3 = x1 / x2\n' > div0.txt
"$SIM" -b div0.txt d.byte counter.txt d.byte > out.txt 2>&1; rc=$?
check "run-time error stops only that task" "$(grep -cF '[pid 1, processor 0] error at instruction 2: division by zero' out.txt),$(word_at d_2.out 0),$rc" "1,10,1"
printf 'print v1\n' > pv.txt
check "print takes an x register" "$("$SIM" -b pv.txt d.byte 2>&1 | grep -c 'usage: print x<n>')" 1
printf 'x1 = 255\nx1 = x1 * 16\n[x1] = v1\n' > far.txt
check "vector write past the end of Data[4096] is an error" "$("$SIM" -b far.txt d.byte 2>&1 | grep -c 'address 4096 is outside Data')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
