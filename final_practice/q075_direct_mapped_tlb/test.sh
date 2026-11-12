#!/usr/bin/env bash
# Q075 checks: a 2-entry direct-mapped TLB per processor (slot = page index & 1), hits and misses per task.
. "$(dirname "$0")/../lib.sh"
build_solution
# pids 1-4 take the 4 processors; pid 5 (pages) waits and loads on processor 0 after tlb_ok (24 instructions)
# ends, so its TLB must be flushed: tlb_ok's slot 1 still says "index 3 -> frame 9", a frame that is free now.
check "5 tasks run without error" "$(run_sim -b tlb_ok.txt tlb_ok_data.byte counter.txt counter_data.byte \
    array_add.txt array_add_data.byte sum_array.txt sum_array_data.byte pages.txt pages_data.byte)" 0
tlb() { grep -c "^\[os\] pid $1 TLB: $2 hits, $3 misses ($4% hits)$" "$RUN/run.txt"; }
check "tlb_ok: 116 lookups, code in slot 0, data in slot 1: 2 misses" "$(tlb 1 114 2 98.3)" 1
check "counter: 176 lookups, 3 misses" "$(tlb 2 173 3 98.3)" 1
check "array_add: 320 lookups, 3 + 2 x 6 misses" "$(tlb 3 305 15 95.3)" 1
check "sum_array: 328 lookups, code and data page 0 fight over slot 0" "$(tlb 4 305 23 93.0)" 1
check "pages: 88 lookups, data pages 1, 3, 7 all in slot 1" "$(tlb 5 79 9 89.8)" 1
check "pages ran on processor 0 after tlb_ok" "$(grep -c 'pid 5 (pages) loaded on processor 0' "$RUN/run.txt")" 1
check "pages: the flushed TLB gave correct results" "$(word_at "$RUN/pages_data_5.out" 0)" 21
check "pages: its data page 1 is in its own page table" "$(word_at "$RUN/pages_data_5.out" 1020)" 7
check "tlb_ok stored 5 in data page 1" "$(word_at "$RUN/tlb_ok_data_1.out" 512)" 5
check "sum_array in the same run" "$(word_at "$RUN/sum_array_data_4.out" 36)" 45
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
