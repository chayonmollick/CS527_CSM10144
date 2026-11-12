#!/usr/bin/env bash
# Q065 checks: after each task finishes, a 16-character frame map (R, ., owner pid or #) is printed.
. "$(dirname "$0")/../lib.sh"
build_solution
# pid 1 sum_array: frames 1 2; pid 2 pages: 3 4; pid 3 tiny: 5 6. Round 1: pages faults -> 7 8 9, tiny ends.
check "sum_array + pages + tiny run without error" \
    "$(run_sim -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte tiny.txt tiny_data.byte)" 0
map() { grep "frames after pid $1 finished:" "$RUN/run.txt" | awk '{ print $NF }'; }
check "after tiny (round 1): hole at 5-6, pages owns 3 4 7 8 9" "$(map 3)" "R1122..222......"
check "after pages (round 2): only sum_array left" "$(map 2)" "R11............."
check "after sum_array: everything free again" "$(map 1)" "R..............."
check "the map has one character per frame (16)" "$(map 1 | awk '{ print length($0) }')" 16
# pids 1..9 = tiny, pid 10 = sum_array: pids 9 and 10 load in round 3 (frames 1 2 and 3 4)
check "10 tasks run without error" \
    "$(run_sim -b $(for i in 1 2 3 4 5 6 7 8 9; do echo tiny.txt tiny_data.byte; done) sum_array.txt sum_array_data.byte)" 0
check "after pid 1: pids 2, 3, 4 still loaded" "$(map 1)" "R..223344......."
check "after pid 9: pid 10 is shown as #" "$(map 9)" "R..##..........."
check "pid 10 (sum_array) result" "$(word_at "$RUN/sum_array_data_10.out" 36)" 45
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
