#!/usr/bin/env bash
# Q064 checks: MAX_FRAMES_PER_TASK = 4; a page fault beyond it stops only that task with "out of memory".
. "$(dirname "$0")/../lib.sh"
build_solution
# pages.txt holds 2 frames, then faults on data pages 1, 3, 7: frames 3 and 4 are allowed, a 5th is not
check "pages.txt fails, so the exit code is 1" "$(run_sim -b pages.txt pages_data.byte sum_array.txt sum_array_data.byte)" 1
check "out of memory on the third fault (store to 3825, instruction 7)" \
    "$(grep -c 'error at instruction 7: out of memory: data page 7 would be frame 5 of this task' "$RUN/run.txt")" 1
check "pages.txt stopped after 8 instructions with 2 page faults" \
    "$(grep -c 'pid 1 (pages) STOPPED WITH AN ERROR on processor 0 after 8 instructions, 2 page fault(s)' "$RUN/run.txt")" 1
check "the stores to pages 1 and 3 happened" "$(word_at "$RUN/pages_data_1.out" 2040)" 7
check "the sum was never stored" "$(word_at "$RUN/pages_data_1.out" 0)" 0
check "the other task kept running (sum_array = 45)" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
# a 2048-byte data file is 4 data pages: with the code page that is 5 frames before the first instruction
awk 'BEGIN { for (i = 0; i < 512; i++) print "00 00 00 00" }' > "$RUN/big_data.byte"
check "a task that needs 5 frames to load is refused" "$(run_sim -b sum_array.txt big_data.byte)" 1
check "with a message" "$(grep -c 'cannot start task: task needs more than MAX_FRAMES_PER_TASK (4) frames' "$RUN/run.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
