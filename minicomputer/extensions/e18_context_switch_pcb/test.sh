#!/usr/bin/env bash
# e18 tests: context switching with a PCB per task on NP = 1 processor.
#   ./test.sh      (builds ./sim, runs everything in a temp directory)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
make -s -C "$HERE" || { echo "build failed"; exit 1; }
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

run() {     # run <test dir> <sim arguments...>: batch run in a fresh copy of the examples; prints the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$dir"/ && (cd "$dir" && "$SIM" -b "$@" > run.txt 2> err.txt)
    echo $?
}

# slices_switches <dir> <pid>: "slices/switches" from that pid's finish line
slices_switches() {
    grep "pid $2 (" "$WORK/$1/run.txt" | grep 'finished' | sed 's/.* \([0-9]*\) slice(s), \([0-9]*\) context switch.*/\1\/\2/'
}

echo "4 counter tasks at once on NP = 1 processor"
check "exit code" "$(run c4 counter.txt counter_data.byte counter_by100.txt counter_data.byte \
                            counter.txt counter_data.byte counter_by100.txt counter_data.byte)" 0
D="$WORK/c4"
check "all 4 resident at once (nobody waits)" "$(grep -c 'is waiting' "$D/run.txt"),$(grep -c 'loaded: page table in slot' "$D/run.txt")" "0,4"
check "page tables in frame 0: PTBR = slot * 10" "$(grep -o 'PTBR = [0-9]*' "$D/run.txt" | awk '{print $3}' | paste -sd, -)" "0,10,20,30"
check "every task ran on processor 0" "$(grep 'finished' "$D/run.txt" | grep -c 'on processor 0')" 4
check "results 10, 1000, 10, 1000" "$(word_at "$D/counter_data_1.out" 0),$(word_at "$D/counter_data_2.out" 0),$(word_at "$D/counter_data_3.out" 0),$(word_at "$D/counter_data_4.out" 0)" "10,1000,10,1000"
check "log.txt: 40 lines" "$(wc -l < "$D/log.txt" | tr -d ' ')" 40
check "log.txt interleaved slice by slice" "$(awk '{print $3}' "$D/log.txt" | paste -sd' ' -)" \
      "1 1 2 2 3 3 4 4 1 1 1 2 2 3 3 3 4 4 1 1 2 2 3 3 4 4 1 1 1 2 2 3 3 3 4 4 2 2 4 4"
check "slices/switches: counter 5/5, counter_by100 6/6" \
      "$(slices_switches c4 1),$(slices_switches c4 2),$(slices_switches c4 3),$(slices_switches c4 4)" "5/5,6/6,5/5,6/6"
check "22 context switches in total" "$(grep -c '22 context switch(es) on 1 processor' "$D/run.txt")" 1
check "exec counters saved in the PCB: counter_1.freq" "$(cut -f1 "$D/counter_1.freq" | paste -sd, -)" "1,.loop,10,10,10,10,1"

echo "A task alone keeps the processor: no switch between its slices"
run alone counter.txt counter_data.byte > /dev/null
check "counter alone: 5 slices, 1 switch" "$(slices_switches alone 1)" "5/1"

echo "Memory, not processors, limits the resident tasks: 8 counters"
set --
for i in 1 2 3 4 5 6 7 8; do set -- "$@" counter.txt counter_data.byte; done
run c8 "$@" > /dev/null
D="$WORK/c8"
check "7 resident (14 of 15 frames), pid 8 waits" "$(grep -c 'loaded: page table' "$D/run.txt"),$(grep -c 'pid 8 is waiting' "$D/run.txt")" "8,1"
check "pid 8 reuses slot 0 of pid 1" "$(grep -c 'pid 8 (counter) loaded: page table in slot 0 (PTBR = 0)' "$D/run.txt")" 1
check "all 8 counters reach 10" "$(for i in 1 2 3 4 5 6 7 8; do word_at "$D/counter_data_$i.out" 0; done | paste -sd, -)" "10,10,10,10,10,10,10,10"
check "36 switches: 7 x 5, then pid 8 alone needs 1" "$(slices_switches c8 8),$(grep -c '36 context switch(es)' "$D/run.txt")" "5/1,1"

echo "Vector registers and flags survive switches (5 programs share one processor)"
check "exit code" "$(run vec array_add.txt array_add_data.byte fir.txt fir_data.byte det3x3.txt det3x3_data.byte \
                             complex_mul.txt complex_mul_data.byte sum_n.txt sum_n_data.byte)" 0
D="$WORK/vec"
check "array_add C[15] = 131" "$(word_at "$D/array_add_data_1.out" 192)" 131
check "fir out[0] = 204, out[15] = 744" "$(word_at "$D/fir_data_2.out" 256),$(word_at "$D/fir_data_2.out" 316)" "204,744"
check "det3x3 = -146" "$(word_at "$D/det3x3_data_3.out" 36)" -146
check "complex_mul = 7 + 26i" "$(word_at "$D/complex_mul_data_4.out" 16),$(word_at "$D/complex_mul_data_4.out" 20)" "7,26"
check "sum_n = 99 (BGE after SUB across switches)" "$(word_at "$D/sum_n_data_5.out" 24)" 99

echo "Demand paging uses the running task's own page table"
run pg pages.txt pages_data.byte counter.txt counter_data.byte > /dev/null
D="$WORK/pg"
check "pages: 3 page faults, counter: 0" "$(grep -c 'pid 1 (pages) finished.* 3 page fault' "$D/run.txt"),$(grep -c 'pid 2 (counter) finished.* 0 page fault' "$D/run.txt")" "1,1"
check "pages results 21 and 7 at 3825" "$(word_at "$D/pages_data_1.out" 0),$(word_at "$D/pages_data_1.out" 3825)" "21,7"

echo "Trace (-t) shows each switch and the new PTBR"
mkdir -p "$WORK/tr" && cp "$EX/counter.txt" "$EX/counter_by100.txt" "$EX/counter_data.byte" "$WORK/tr"/
(cd "$WORK/tr" && "$SIM" -t -b counter.txt counter_data.byte counter_by100.txt counter_data.byte > run.txt 2> err.txt)
check "first switches" "$(grep 'context switch' "$WORK/tr/err.txt" | head -3 | sed 's/.*: //' | paste -sd'|' -)" \
      "idle -> pid 1 (PTBR = 0)|pid 1 -> pid 2 (PTBR = 10)|pid 2 -> pid 1 (PTBR = 0)"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
