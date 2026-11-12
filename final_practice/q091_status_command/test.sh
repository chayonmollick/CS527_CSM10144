#!/usr/bin/env bash
# Q091 checks: the shell command "status" (round, tasks per state, free frames, busy processors).
. "$(dirname "$0")/../lib.sh"
statuses() { grep -o '\[status\].*' "$RUN/run.txt"; }      # the status lines (after any "$ " prompt)
build_solution

printf 'status\nexit\n' > "$RUN/cmds0.txt"
check "status with no tasks runs" "$(run_sim < "$RUN/cmds0.txt")" 0
check "nothing has run yet" "$(statuses)" \
      "[status] round 0: 0 finished, 0 running, 0 waiting; 15 of 15 frames free; 0 of 4 processors busy: p0=free p1=free p2=free p3=free"

# quick (pid 1) ends in round 1; counters pids 2-4 run 5 rounds; pid 5 waits. The shell reads the
# whole file right after round 1, so both status lines describe the end of round 1.
printf 'status\ncounter.txt counter_data.byte\nstatus\nexit\n' > "$RUN/cmds1.txt"
check "session with 5 tasks runs without error" "$(run_sim quick.txt quick_data.byte counter.txt counter_data.byte \
      counter.txt counter_data.byte counter.txt counter_data.byte counter.txt counter_data.byte < "$RUN/cmds1.txt")" 0
check "status after round 1" "$(statuses | head -1)" \
      "[status] round 1: 1 finished, 3 running, 1 waiting; 9 of 15 frames free; 3 of 4 processors busy: p0=free p1=pid2 p2=pid3 p3=pid4"
check "after typing a task: waiting pid 5 took p0, the new pid 6 waits" "$(statuses | tail -1)" \
      "[status] round 1: 1 finished, 4 running, 1 waiting; 7 of 15 frames free; 4 of 4 processors busy: p0=pid5 p1=pid2 p2=pid3 p3=pid4"
check "all 6 tasks finished" "$(grep -c 'finished on processor' "$RUN/run.txt")" 6

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
