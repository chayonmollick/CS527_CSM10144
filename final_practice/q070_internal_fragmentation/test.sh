#!/usr/bin/env bash
# Q070 checks: the loader reports the unused bytes in each task's last instruction page and last data page.
. "$(dirname "$0")/../lib.sh"
build_solution
frag() { grep -c "pid $1 internal fragmentation: $2 byte(s) unused in the last instruction page + $3 in the last data page = $4$" "$RUN/run.txt"; }
check "4 example tasks run without error" \
    "$(run_sim -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte array_add.txt array_add_data.byte fir.txt fir_data.byte)" 0
check "sum_array: 13 instr = 52 B, data 40 B -> 460 + 472" "$(frag 1 460 472 932)" 1
check "pages: 15 instr = 60 B, data 4 B -> 452 + 508" "$(frag 2 452 508 960)" 1
check "array_add: 18 instr = 72 B, data 132 B -> 440 + 380" "$(frag 3 440 380 820)" 1
check "fir: 53 instr = 212 B, data 132 B -> 300 + 380" "$(frag 4 300 380 680)" 1
# data files of exactly one page (128 words) and of one page + 4 bytes (129 words)
awk 'BEGIN { for (i = 0; i < 128; i++) print "00 00 00 00" }' > "$RUN/full_data.byte"
awk 'BEGIN { for (i = 0; i < 129; i++) print "00 00 00 00" }' > "$RUN/over_data.byte"
check "counter on 512 and 516-byte data files" "$(run_sim -b counter.txt full_data.byte counter.txt over_data.byte)" 0
check "512 bytes fill the data page exactly: 0 unused" "$(frag 1 484 0 484)" 1
check "516 bytes need 2 data pages, 4 bytes used in the last" "$(frag 2 484 508 992)" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
