#!/usr/bin/env bash
# Q079 checks: the shell command ps lists pid, program, state, processor and instructions so far.
. "$(dirname "$0")/../lib.sh"
build_solution
row() { awk -v P="$1" '$1 == P && NF == 5 {print $2, $3, $4, $5}' "$RUN/run.txt"; }   # one ps line
rows() { awk '$1 ~ /^[0-9]+$/ && NF == 5' "$RUN/run.txt" | wc -l | tr -d ' '; }
# 5 tasks on 4 processors; stdin is a file, so the shell runs 'ps' right after round 1.
printf 'ps\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" counter.txt counter_data.byte tiny.txt counter_data.byte counter_by100.txt counter_data.byte \
                     counter.txt counter_data.byte counter_by100.txt counter_data.byte < session.in > run.txt 2>&1)
check "the session ends without error" "$?" 0
check "ps lists all 5 tasks" "$(rows)" 5
check "pid 1: READY on processor 0, 10 instructions (one slice)" "$(row 1)" "counter READY 0 10"
check "pid 2: DONE after 3 instructions, no processor" "$(row 2)" "tiny DONE - 3"
check "pid 3: READY on processor 2, 10 instructions" "$(row 3)" "counter_by100 READY 2 10"
check "pid 4: READY on processor 3, 10 instructions" "$(row 4)" "counter READY 3 10"
check "pid 5: WAITING, no processor, 0 instructions" "$(row 5)" "counter_by100 WAITING - 0"
check "afterwards every task finishes (pid 2 = 7, pid 5 = 1000)" \
      "$(word_at "$RUN/counter_data_2.out" 0),$(word_at "$RUN/counter_data_5.out" 0)" "7,1000"
# Only the shell: the first ps has no tasks; the second sees a task that has not run yet.
printf 'ps\ncounter.txt counter_data.byte\nps\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" < session.in > run.txt 2>&1)
check "two ps headers, one task line in all" "$(grep -c 'PID  PROGRAM' "$RUN/run.txt"),$(rows)" "2,1"
check "a task typed in the shell has run 0 instructions before round 1" "$(row 1)" "counter READY 0 0"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
