#!/usr/bin/env bash
# Q067 checks: ZERO_FRAMES 1 (default) clears a demand-mapped frame; with 0 a later task reads a finished task's data.
. "$(dirname "$0")/../lib.sh"
build_solution
# pid 1 (secret) writes 9999 into data page 1 = frame 9 and ends in round 1, freeing frames 1, 2, 9.
# pids 2-4 keep processors 1-3 busy without page faults; pid 5 (snoop) waits, loads in round 2 on
# frames 1 and 2, and its first touch of data page 1 gets the lowest free frame: 9 again.
TASKS="secret.txt secret_data.byte sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte counter.txt counter_data.byte snoop.txt snoop_data.byte"
check "ZERO_FRAMES 1: the 5 tasks run without error" "$(run_sim -b $TASKS)" 0
check "snoop had to wait until secret finished" "$(grep -c 'pid 5 is waiting' "$RUN/run.txt")" 1
check "secret wrote 9999 at 512" "$(word_at "$RUN/secret_data_1.out" 512)" 9999
check "ZERO_FRAMES 1: snoop's new page reads 0" "$(word_at "$RUN/snoop_data_5.out" 0)" 0
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
# the same code built with the switch off
out="$(cd "$WORK/sim" && make clean > /dev/null && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DZERO_FRAMES=0" 2>&1)"
check "ZERO_FRAMES 0 builds without warnings" "$(echo "$out" | grep -cE "warning|error")" 0
check "ZERO_FRAMES 0: the 5 tasks run without error" "$(run_sim -b $TASKS)" 0
check "ZERO_FRAMES 0: snoop reads the finished task's secret" "$(word_at "$RUN/snoop_data_5.out" 0)" 9999
check "ZERO_FRAMES 0: tasks that write before reading are still right" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
finish
