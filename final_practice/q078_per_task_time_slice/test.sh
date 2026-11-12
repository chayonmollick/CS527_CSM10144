#!/usr/bin/env bash
# Q078 checks: an optional time slice after the data file, per task, on the command line and in the shell.
. "$(dirname "$0")/../lib.sh"
build_solution
pids() { awk '{print $3}' "$RUN/log.txt" | paste -sd, -; }            # pid of every log.txt line
finish_order() { grep 'finished on processor' "$RUN/run.txt" | awk '{print $3}' | paste -sd, -; }
check "command line: 'prog data 100' runs" "$(run_sim -b counter.txt counter_data.byte counter.txt counter_data.byte 100)" 0
check "pid 1 keeps the default slice 10" "$(grep -c 'pid 1 created: counter.txt counter_data.byte (7 instructions, time slice 10)' "$RUN/run.txt")" 1
check "pid 2 gets slice 100" "$(grep -c 'pid 2 created: counter.txt counter_data.byte (7 instructions, time slice 100)' "$RUN/run.txt")" 1
check "pid 2 runs all 43 instructions in its first turn" "$(awk '{print $3}' "$RUN/log.txt" | uniq | paste -sd, -)" "1,2,1"
check "so pid 2 finishes first" "$(finish_order)" "2,1"
: > "$RUN/log.txt"                                                  # log.txt is opened in append mode
# Shell: stdin is a file, so every line is read before round 1 (no task is given on the command line).
printf 'counter.txt counter_data.byte 4\ncounter.txt counter_data.byte\ncounter.txt counter_data.byte 0\ncounter.txt counter_data.byte x\nexit\n' > "$RUN/session.in"
(cd "$RUN" && "$SIM" < session.in > run.txt 2>&1)
check "shell: 'prog data 4' gives slice 4" "$(grep -c 'pid 1 created: .*time slice 4)' "$RUN/run.txt")" 1
check "shell: 'prog data' keeps slice 10" "$(grep -c 'pid 2 created: .*time slice 10)' "$RUN/run.txt")" 1
check "shell: slice 0 is refused" "$(grep -c 'cannot start task: the time slice must be at least 1' "$RUN/run.txt")" 1
check "shell: a third word that is not a number gives the usage" "$(grep -c 'usage:' "$RUN/run.txt")" 1
check "shell: 1 print per slice of 4, 2 or 3 per slice of 10" "$(pids)" "1,2,2,1,2,2,2,1,2,2,1,2,2,2,1,1,1,1,1,1"
check "shell: pid 2 (round 5) finishes before pid 1 (round 11)" "$(finish_order)" "2,1"
check "results unchanged" "$(word_at "$RUN/counter_data_1.out" 0),$(word_at "$RUN/counter_data_2.out" 0)" "10,10"
check "a program without its data file: usage, exit code 1" \
      "$(run_sim -b counter.txt counter_data.byte counter.txt),$(grep -c '^usage' "$RUN/run.txt")" "1,1"
run_sim -b -t counter.txt counter_data.byte 20 counter_by100.txt counter_data.byte > /dev/null
runs() { grep '^\[pid' "$RUN/run.txt" | awk '{print $2}' | uniq -c | awk -v P="$1" '$2 == P {print $1}' | paste -sd, -; }
check "-t: pid 1 (slice 20) runs 20 + 20 + 3 instructions" "$(runs 1)" "20,20,3"
check "-t: pid 2 (default) runs 10 per turn while pid 1 runs" "$(runs 2 | cut -d, -f1-2)" "10,10"
(cd "$RUN" && "$SIM" -c counter.txt > listing.txt)
check "-c still prints the listing" "$(grep -c 'BNE -3' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
