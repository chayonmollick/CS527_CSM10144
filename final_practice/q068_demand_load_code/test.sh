#!/usr/bin/env bash
# Q068 checks: only instruction page 0 is loaded at the start; page 1 is copied from the program on its first fetch.
. "$(dirname "$0")/../lib.sh"
build_solution
check "long.txt (131 instructions) runs without error" "$(run_sim -b long.txt long_data.byte)" 0
check "loaded with 1 instruction page, 13 frames left (lab6: 2 and 12)" \
    "$(grep -c 'pid 1 (long) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left' "$RUN/run.txt")" 1
check "page 1 loaded on its first fetch, into frame 3" \
    "$(grep -c '^\[os\] pid 1: instruction page 1 loaded on its first fetch (frame 3)$' "$RUN/run.txt")" 1
check "all 131 instructions ran, with 1 page fault" "$(grep -c 'after 131 instructions, 1 page fault(s)' "$RUN/run.txt")" 1
check "the code in page 1 is the program's (128 increments)" "$(word_at "$RUN/long_data_1.out" 0)" 128
# jump.byte: x1 = 1, BAL +127 -> instruction 128, but the program image is only 12 bytes
check "a jump to a page the program does not have still fails" "$(run_sim -b jump.byte long_data.byte)" 1
check "with the old error message, at instruction 128" \
    "$(grep -c 'error at instruction 128: fetch from unmapped instruction page 1' "$RUN/run.txt")" 1
check "and nothing was loaded for it" "$(grep -c 'after 2 instructions, 0 page fault(s)' "$RUN/run.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
