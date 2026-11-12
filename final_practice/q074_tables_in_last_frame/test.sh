#!/usr/bin/env bash
# Q074 checks: page tables in the last frame (15), frame 0 usable by tasks, "not mapped" marker = 15.
. "$(dirname "$0")/../lib.sh"
build_solution
# pids 1-4 = tiny, pages, sum_array, counter load into frames 0-1, 2-3, 4-5, 6-7; pid 5 (tiny) waits for a processor.
# Round 1: tiny ends and frees 0, 1; pages demand-maps data pages 1, 3, 7 -> frames 0, 1, 8. Round 2: pid 5 -> 9, 10.
check "5 tasks run without error" "$(run_sim -b tiny.txt tiny_data.byte pages.txt pages_data.byte \
    sum_array.txt sum_array_data.byte counter.txt counter_data.byte tiny.txt tiny_data.byte)" 0
table() { grep -c "pid $1 on processor $2: page table at PTBR $3 (frame 15), instruction page 0 in frame $4$" "$RUN/run.txt"; }
check "pid 1: PTBR = 15*512 + 0 = 7680, code in frame 0" "$(table 1 0 7680 0)" 1
check "pid 2: PTBR = 7680 + 10 = 7690, code in frame 2" "$(table 2 1 7690 2)" 1
check "pid 3: PTBR = 7700, code in frame 4" "$(table 3 2 7700 4)" 1
check "pid 4: PTBR = 7710, code in frame 6" "$(table 4 3 7710 6)" 1
check "pid 5 (round 2) gets frame 9: frames 0 and 1 went to pages.txt" "$(table 5 0 7680 9)" 1
left() { grep "pid $1 (.*) loaded" "$RUN/run.txt" | sed 's/.*, \([0-9]*\) frame(s) left/\1/'; }
check "still 15 usable frames: 13 11 9 7 left, then 4" "$(left 1) $(left 2) $(left 3) $(left 4) $(left 5)" "13 11 9 7 4"
check "pages: data page 1 lived in frame 0 and was written back" "$(word_at "$RUN/pages_data_2.out" 1020)" 7
check "pages: result 7 + 7 + 7" "$(word_at "$RUN/pages_data_2.out" 0)" 21
check "counter in the same run" "$(word_at "$RUN/counter_data_4.out" 0)" 10
check "the second tiny" "$(word_at "$RUN/tiny_data_5.out" 0)" 5
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
