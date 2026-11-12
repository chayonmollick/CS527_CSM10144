#!/usr/bin/env bash
# Q069 checks: tasks running the same byte code share its frames; a frame is freed when its last user ends.
. "$(dirname "$0")/../lib.sh"
build_solution
# pid 1 sum_array on sum_n_data (43 instructions), pid 2 sum_array (71), pid 3 + pid 4 counter (43 each),
# pid 5 pages waits for a processor and loads in round 6, while pid 2 still runs on the shared frame 1.
check "5 tasks run without error" "$(run_sim -b sum_array.txt sum_n_data.byte sum_array.txt sum_array_data.byte \
    counter.txt counter_data.byte counter.txt counter_data.byte pages.txt pages_data.byte)" 0
check "pid 2 shares sum_array's code frame with pid 1" "$(grep -c '^\[os\] pid 2 shares 1 instruction frame(s) with pid 1$' "$RUN/run.txt")" 1
check "pid 4 shares counter's code frame with pid 3" "$(grep -c '^\[os\] pid 4 shares 1 instruction frame(s) with pid 3$' "$RUN/run.txt")" 1
check "no other task shares" "$(grep -c ' shares ' "$RUN/run.txt")" 2
left() { grep "pid $1 (.*) loaded" "$RUN/run.txt" | sed 's/.*, \([0-9]*\) frame(s) left/\1/'; }
check "frames left after each load: 13 12 10 9, pid 5 later 11" "$(left 1) $(left 2) $(left 3) $(left 4) $(left 5)" "13 12 10 9 11"
check "pid 1 ends: frame 1 stays (pid 2 uses it)" "$(grep -c '^\[os\] pid 1: frame 1 is still used by 1 other task(s)$' "$RUN/run.txt")" 1
check "pid 3 ends: frame 4 stays (pid 4 uses it)" "$(grep -c '^\[os\] pid 3: frame 4 is still used by 1 other task(s)$' "$RUN/run.txt")" 1
check "the last users free them (no message for pids 2, 4)" "$(grep -c 'is still used' "$RUN/run.txt")" 2
check "pid 1: sum of sum_n_data (10 - 1 + 20 + 30 + 40)" "$(word_at "$RUN/sum_n_data_1.out" 20)" 99
check "pid 2: its shared code was not overwritten by pid 5" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
check "pid 3 counter" "$(word_at "$RUN/counter_data_3.out" 0)" 10
check "pid 4 counter" "$(word_at "$RUN/counter_data_4.out" 0)" 10
check "pid 5 pages" "$(word_at "$RUN/pages_data_5.out" 0)" 21
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
