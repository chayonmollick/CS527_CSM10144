#!/usr/bin/env bash
# Q082 checks: the shell command queues shows the ready queue and the waiting queue, front first.
. "$(dirname "$0")/../lib.sh"
build_solution
q() { grep "$1 queue:" "$RUN/run.txt" | sed -n "$2p" | sed 's/.*queue: *//'; }   # q ready|waiting <n-th time>
# 6 tasks on 4 processors: pids 5 and 6 wait. stdin is a file, so the shell runs after round 1.
printf 'queues\ncounter.txt counter_data.byte\nqueues\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" tiny.txt counter_data.byte counter.txt counter_data.byte counter.txt counter_data.byte \
                     counter.txt counter_data.byte counter_by100.txt counter_data.byte counter_by100.txt counter_data.byte \
                     < session.in > run.txt 2>&1)
check "the session ends without error" "$?" 0
check "after round 1: pid 1 has finished, 2 3 4 are ready" "$(q ready 1)" "2 3 4"
check "pids 5 and 6 still wait: tasks are admitted at the start of a round" "$(q waiting 1)" "5 6"
check "typing a task loads pid 5 on the free processor 0" "$(grep -c 'pid 5 (counter_by100) loaded on processor 0' "$RUN/run.txt")" 1
check "then pid 5 is at the end of the ready queue" "$(q ready 2)" "2 3 4 5"
check "and the new pid 7 waits behind pid 6" "$(q waiting 2)" "6 7"
check "all 7 tasks finish (pid 7 counts to 10)" "$(grep -c 'finished on processor' "$RUN/run.txt"),$(word_at "$RUN/counter_data_7.out" 0)" "7,10"
printf 'queues\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" < session.in > run.txt 2>&1)
check "with no tasks both queues are empty" "$(q ready 1) | $(q waiting 1)" "(empty) | (empty)"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
