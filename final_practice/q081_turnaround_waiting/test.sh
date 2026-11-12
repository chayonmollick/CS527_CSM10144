#!/usr/bin/env bash
# Q081 checks: turnaround and waiting time in scheduler rounds, per task and on average.
. "$(dirname "$0")/../lib.sh"
build_solution
row() { awk -v P="$1" '$1 == "[os]" && $2 == P && NF == 7 {print $4, $5, $6, $7}' "$RUN/run.txt"; }   # arrival finish turnaround waiting
FOUR="counter.txt counter_data.byte counter_by100.txt counter_data.byte counter.txt counter_data.byte counter_by100.txt counter_data.byte"
check "5 tasks on 4 processors run without error" "$(run_sim -b $FOUR counter.txt counter_data.byte)" 0
check "pid 1 (counter, 5 slices): arrival 0, finish 5, turnaround 5, waiting 0" "$(row 1)" "0 5 5 0"
check "pid 2 (counter_by100, 6 slices): finish 6, turnaround 6, waiting 0" "$(row 2)" "0 6 6 0"
check "pid 3 (counter): finish 5" "$(row 3)" "0 5 5 0"
check "pid 4 (counter_by100): finish 6" "$(row 4)" "0 6 6 0"
check "pid 5 waits 5 rounds for a processor: finish 10, turnaround 10, waiting 5" "$(row 5)" "0 10 10 5"
check "averages: turnaround 32/5, waiting 5/5" "$(grep -c 'average turnaround 6.40 rounds, average waiting 1.00 rounds (5 tasks)' "$RUN/run.txt")" 1
# The 5th task typed in the shell: stdin is a file, read after round 1, so it arrives in round 1.
printf 'counter.txt counter_data.byte\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" $FOUR < session.in > run.txt 2>&1)
check "shell task: arrival 1, finish 10, turnaround 9, waiting 4" "$(row 5)" "1 10 9 4"
check "averages: turnaround 31/5, waiting 4/5" "$(grep -c 'average turnaround 6.20 rounds, average waiting 0.80 rounds (5 tasks)' "$RUN/run.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
check "sum_array alone: 71 instructions = 8 slices, never waits" "$(row 1)" "0 8 8 0"
finish
