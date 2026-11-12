#!/usr/bin/env bash
# Lab 5 tests: build ./sim, run the examples (each in a fresh temp directory) and check the
# results, the page tables, the page-fault counts and that every frame is freed again.
#   ./test.sh                        (or: make test)
#   SIM=/path/to/other/sim ./test.sh  check another build, e.g. a modified copy, instead
# Prints "N passed, M failed"; the exit code is non-zero when a check fails.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/examples"
if [ -z "${SIM:-}" ]; then
    SIM="$HERE/sim"
    (cd "$HERE" && make -s --no-print-directory sim) || { echo "build failed"; exit 1; }
fi
case "$SIM" in /*) ;; *) SIM="$PWD/$SIM" ;; esac      # the tests run in other directories
[ -x "$SIM" ] || { echo "no simulator at $SIM"; exit 1; }
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

# word_at <file> <byte address>: signed 32-bit little-endian word from a .byte/.out file
# (the same helper as ../minicomputer/tests/run_tests.sh)
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

# count <file> <text>: how many lines contain text (no regular expression)
count() { grep -c -F -- "$2" "$1"; }

# line_no <file> <text>: number of the first line that contains text, 0 if none
line_no() { awk -v T="$2" 'index($0, T) { print NR; found = 1; exit } END { if (!found) print 0 }' "$1"; }

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <test dir> <sim arguments...>: batch mode, output in run.txt; prints the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$dir"/ && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

echo "Labs 1-3 programs on paged memory"
check "sum_array exit code" "$(run sum sum_array.txt sum_array_data.byte)" 0
check "sum_array (handout program) [36] = 1 + .. + 9" "$(word_at "$WORK/sum/sum_array_data_1.out" 36)" 45
expected_code="0F 01 00 00|0F 02 00 00|0F 03 00 00|05 04 00 03|02 0F 01 04|10 00 00 06|09 03 03 04|05 06 00 03|01 02 02 06|09 01 01 01|1E 00 00 FA|06 03 00 02|00 00 00 00"
check "sum_array byte code = handout listing" "$(paste -sd'|' "$WORK/sum/program.byte")" "$expected_code"
run vec array_add.txt array_add_data.byte > /dev/null
check "array_add C[0] = 1 + 100" "$(word_at "$WORK/vec/array_add_data_1.out" 132)" 101
check "array_add C[15] = 16 + 115" "$(word_at "$WORK/vec/array_add_data_1.out" 192)" 131
run fir fir.txt fir_data.byte > /dev/null
check "fir out[0] = 1*1 + 2*2 + .. + 8*8" "$(word_at "$WORK/fir/fir_data_1.out" 256)" 204
check "fir out[15] = 36*15 + 204" "$(word_at "$WORK/fir/fir_data_1.out" 316)" 744
check "no .freq report (that is Lab 6)" "$(ls "$WORK/sum" "$WORK/vec" "$WORK/fir" | grep -c '\.freq$')" 0

echo "Lab 4 on paged memory: 5 tasks, 4 processors, print, round robin"
run rr counter.txt counter_data.byte counter.txt counter_data.byte counter.txt counter_data.byte \
       counter.txt counter_data.byte counter.txt counter_data.byte > /dev/null
check "pid 5 waits for a processor (frames are not the problem)" "$(count "$WORK/rr/run.txt" 'pid 5 is waiting: no free processor')" 1
check "counter final value (pid 5)" "$(word_at "$WORK/rr/counter_data_5.out" 0)" 10
check "50 print lines in log.txt" "$(wc -l < "$WORK/rr/log.txt" | tr -d ' ')" 50
check "round robin (assumes NP = 4): each slice prints twice, pids 1-4 in turn" "$(head -8 "$WORK/rr/log.txt" | awk '{print $3}' | paste -sd, -)" "1,1,2,2,3,3,4,4"

echo "Lab 5: the loader maps the pages of the files; other data pages on first touch"
run pg pages.txt pages_data.byte > /dev/null
check "pages: loader maps 1 instruction + 1 data page (frames 1, 2)" "$(count "$WORK/pg/run.txt" 'loaded on processor 0: page table [1 - | 2 - - - - - - -], 13 frame(s) free')" 1
check "pages: 3 page faults" "$(count "$WORK/pg/run.txt" '15 instructions, 3 page fault(s)')" 1
check "pages: data pages 1, 3, 7 got frames 3, 4, 5" "$(count "$WORK/pg/run.txt" 'page table [1 - | 2 3 - 4 - - - 5] reset: 5 frame(s) freed')" 1
check "pages: sum of the three pages at 0" "$(word_at "$WORK/pg/pages_data_1.out" 0)" 21
check "pages: value written at 3825 (data page 7)" "$(word_at "$WORK/pg/pages_data_1.out" 3825)" 7
check "pages: all 15 frames free at the end" "$(count "$WORK/pg/run.txt" '15 of 15 frames free')" 1
run ta touch_all.txt touch_all_data.byte > /dev/null
check "touch_all: 7 page faults" "$(count "$WORK/ta/run.txt" '7 page fault(s)')" 1
check "touch_all: every data page mapped (9 frames)" "$(count "$WORK/ta/run.txt" 'page table [1 - | 2 3 4 5 6 7 8 9] reset: 9 frame(s) freed, 15 free')" 1
check "touch_all: sum 1 + .. + 7 at 0" "$(word_at "$WORK/ta/touch_all_data_1.out" 0)" 28
check "touch_all: 7 at 3584 (data page 7)" "$(word_at "$WORK/ta/touch_all_data_1.out" 3584)" 7

echo "Lab 5: a word across a page boundary is translated byte by byte"
run cp1 cross_page.txt cross_page_data.byte > /dev/null
check "cross_page: word at 510 reads back (0x01020304)" "$(word_at "$WORK/cp1/cross_page_data_1.out" 0),$(word_at "$WORK/cp1/cross_page_data_1.out" 510)" "16909060,16909060"
check "cross_page: bytes 512-513 needed data page 1 (1 page fault)" "$(count "$WORK/cp1/run.txt" 'page table [1 - | 2 3 - - - - - -] reset')" 1
run cp2 pages.txt pages_data.byte cross_page.txt cross_page_data.byte > /dev/null
check "after pages.txt frees frame 1: the word is split over frames 4 and 1" "$(count "$WORK/cp2/run.txt" 'page table [3 - | 4 1 - - - - - -] reset')" 1
check "and still reads back" "$(word_at "$WORK/cp2/cross_page_data_2.out" 0)" 16909060

echo "Lab 5: two tasks, same logical address, different frames"
run same same_addr.txt same_addr_a.byte same_addr.txt same_addr_b.byte > /dev/null
check "pid 1 reads back its own value from 2000" "$(word_at "$WORK/same/same_addr_a_1.out" 4)" 11
check "pid 2 reads back its own value from 2000" "$(word_at "$WORK/same/same_addr_b_2.out" 4)" 22
check "data page 3 is frame 5 in pid 1 and frame 6 in pid 2" \
      "$(count "$WORK/same/run.txt" '[1 - | 2 - - 5 - - - -] reset'),$(count "$WORK/same/run.txt" '[3 - | 4 - - 6 - - - -] reset')" "1,1"

echo "Lab 5: four programs share physical memory"
run mix pages.txt pages_data.byte sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte fir.txt fir_data.byte > /dev/null
check "mix: pages" "$(word_at "$WORK/mix/pages_data_1.out" 0)" 21
check "mix: sum_array" "$(word_at "$WORK/mix/sum_array_data_2.out" 36)" 45
check "mix: array_add" "$(word_at "$WORK/mix/array_add_data_3.out" 192)" 131
check "mix: fir" "$(word_at "$WORK/mix/fir_data_4.out" 316)" 744
check "mix: pages' faulted pages got frames 9, 10, 11 (after the others' frames)" "$(count "$WORK/mix/run.txt" 'page table [1 - | 2 9 - 10 - - - 11] reset')" 1
check "mix: all 15 frames free at the end" "$(count "$WORK/mix/run.txt" '15 of 15 frames free')" 1

echo "Lab 5: frames run out, so a task waits for frames"
rc=$(run wait big_sum.txt big_sum_short.byte big_sum.txt big_sum_data.byte big_sum.txt big_sum_data.byte big_sum.txt big_sum_data.byte)
R="$WORK/wait/run.txt"
check "exit code" "$rc" 0
check "pids 1-3 take all 15 frames (5 each)" "$(count "$R" 'loaded on processor 2: page table [11 - | 12 13 14 15 - - - -], 0 frame(s) free')" 1
check "pid 4 waits for frames although processor 3 is free" "$(count "$R" 'pid 4 is waiting: needs 5 frame(s), 0 free')" 1
l1=$(line_no "$R" 'pid 1 (big_sum) finished'); l4=$(line_no "$R" 'pid 4 (big_sum) loaded'); l2=$(line_no "$R" 'pid 2 (big_sum) finished')
check "pid 4 starts after pid 1 finished, while pid 2 still runs" "$([ "$l1" -gt 0 ] && [ "$l1" -lt "$l4" ] && [ "$l4" -lt "$l2" ] && echo yes || echo no)" yes
check "pid 4 reuses the frames pid 1 freed (1-5)" "$(count "$R" 'pid 4 (big_sum) loaded on processor 0: page table [1 - | 2 3 4 5 - - - -]')" 1
check "pid 1: sum of 1..400" "$(word_at "$WORK/wait/big_sum_short_1.out" 1604)" 80200
check "pids 2, 3, 4: sum of 1..500" "$(word_at "$WORK/wait/big_sum_data_2.out" 2004),$(word_at "$WORK/wait/big_sum_data_3.out" 2004),$(word_at "$WORK/wait/big_sum_data_4.out" 2004)" "125250,125250,125250"
check "all 15 frames free at the end" "$(count "$R" '15 of 15 frames free')" 1

echo "Lab 5: no free frame at a first touch: ERROR, only that task stops"
rc=$(run err touch_all.txt touch_all_data.byte touch_all.txt touch_all_data.byte)
R="$WORK/err/run.txt"
check "exit code 1 (a task failed)" "$rc" 1
check "pid 2 gets the ERROR at data page 6" "$(count "$R" '[pid 2, processor 1] error at instruction 7: ERROR: no free physical frame for data page 6')" 1
check "pid 2 is stopped and its 7 frames are freed" "$(count "$R" 'page table [3 - | 4 6 8 10 12 14 - -] reset: 7 frame(s) freed')" 1
check "pid 1 still finishes: sum 28" "$(word_at "$WORK/err/touch_all_data_1.out" 0)" 28
check "pid 1's data page 7 reuses frame 3, freed by pid 2" "$(count "$R" 'page table [1 - | 2 5 7 9 11 13 15 3] reset')" 1
check "all 15 frames free at the end" "$(count "$R" '15 of 15 frames free')" 1

echo "Running a precompiled .byte program"
mkdir -p "$WORK/byte" && cp "$WORK/sum/program.byte" "$WORK/byte/sum.byte" && cp "$EX/sum_array_data.byte" "$WORK/byte/"
(cd "$WORK/byte" && "$SIM" -b sum.byte sum_array_data.byte > run.txt 2>&1)
check "sum.byte gives the same result" "$(word_at "$WORK/byte/sum_array_data_1.out" 36)" 45

echo "Shell"
mkdir -p "$WORK/sh" && cp "$EX"/* "$WORK/sh"/
(cd "$WORK/sh" && printf 'counter.txt counter_data.byte\nbad command here\nexit\n' | "$SIM" > run.txt 2>&1)
check "task started from the shell finished" "$(word_at "$WORK/sh/counter_data_1.out" 0)" 10
check "shell rejects a bad command" "$(count "$WORK/sh/run.txt" 'usage')" 1

echo "Errors"
mkdir -p "$WORK/err2" && cd "$WORK/err2" && printf '00 00 00 00\n' > d.byte
printf 'x1 = x300 + 1\n' > badreg.txt
"$SIM" -b badreg.txt d.byte > out.txt 2>&1; rc=$?
check "compile error: bad register (exit code 1)" "$rc" 1
check "compile error names file and line" "$(count out.txt 'badreg.txt:1: register x300 does not exist')" 1
printf 'x1 = 5\nx2 = 0\nx3 = x1 / x2\n' > div0.txt
"$SIM" -b div0.txt d.byte > out.txt 2>&1; rc=$?
check "runtime error: division by zero stops the task" "$(count out.txt 'division by zero'),$rc" "1,1"
printf 'x1 = 255\nx1 = x1 * 20\nx2 = [x1]\n' > far.txt
"$SIM" -b far.txt d.byte > out.txt 2>&1
check "data address 5100 is outside the logical data space" "$(count out.txt 'address 5100 is outside the logical data space')" 1
printf '0F 01 00 00\n1E 00 00 7F\n' > jump.byte
"$SIM" -b jump.byte d.byte > out.txt 2>&1
check "jump to 512: instruction page 1 is not mapped" "$(count out.txt 'fetch from unmapped instruction page 1')" 1
awk 'BEGIN { for (i = 0; i < 1025; i++) print "00 00 00 00" }' > huge.byte
"$SIM" -b far.txt huge.byte > out.txt 2>&1
check "data file larger than the 4096-byte data space is refused" "$(count out.txt 'is larger than the data space')" 1
check "BNE to an unknown label is a compile error" "$(printf 'BNE .nowhere\n' > nolabel.txt; "$SIM" -b nolabel.txt d.byte 2>&1 | grep -c 'unknown label .nowhere')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
