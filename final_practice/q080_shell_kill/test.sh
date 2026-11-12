#!/usr/bin/env bash
# Q080 checks: kill <pid> ends a running task (frees its processor and frames) or removes a waiting one.
. "$(dirname "$0")/../lib.sh"
build_solution
# 6 tasks on 4 processors: pids 5 and 6 wait. stdin is a file, so the shell runs the kills after round 1.
printf 'kill 1\nkill 5\nkill 1\nkill 9\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" long_job.txt long_job_data.byte counter.txt counter_data.byte counter_by100.txt counter_data.byte \
                     counter.txt counter_data.byte counter_by100.txt counter_data.byte counter.txt counter_data.byte \
                     < session.in > run.txt 2>&1)
check "a kill is not an error: exit code 0" "$?" 0
OUT="$RUN/run.txt"
check "kill 1: the long job stops after its first slice" "$(grep -c 'pid 1 (long_job) KILLED on processor 0 after 10 instructions' "$OUT")" 1
check "its output is written as it was: [0] = 2 loop turns" "$(word_at "$RUN/long_job_data_1.out" 0)" 2
check "kill 5: a waiting task is removed" "$(grep -c 'pid 5 (counter_by100) KILLED while waiting' "$OUT")" 1
check "pid 5 never ran: no log lines, no .out file" "$(grep -c 'Process id: 5 ' "$RUN/log.txt"),$(ls "$RUN" | grep -c '_5\.out$')" "0,0"
check "kill of a finished pid (1) and of an unknown pid (9) is refused" "$(grep -c 'kill: no running or waiting task' "$OUT")" 2
check "round 2 loads pid 6 on the freed processor 0" "$(grep -c 'pid 6 (counter) loaded on processor 0' "$OUT")" 1
check "with pid 1's frames back: 7 frames left, as before the kill" "$(grep 'pid 6 (counter) loaded' "$OUT" | grep -c ', 7 frame(s) left')" 1
check "the other tasks finish normally (pid 6 counts to 10)" "$(grep -c 'finished on processor' "$OUT"),$(word_at "$RUN/counter_data_6.out" 0)" "4,10"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
