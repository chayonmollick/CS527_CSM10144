#!/usr/bin/env bash
# Q089 checks: the highest-priority waiting task gets the next free processor; waiting adds 1 per round.
. "$(dirname "$0")/../lib.sh"
starts() { awk '/starts with priority/ { printf "%s%s:%s", s, $3, $NF; s = " " } END { print "" }' "$RUN/run.txt"; }
build_solution

# A: jobs of 2, 4, 6, 6 rounds hold the 4 processors; three 1-round jobs wait with priorities 1, 4, 2.
check "scenario A runs without error" "$(run_sim -b spin.txt r2.byte spin.txt r4.byte spin.txt r6.byte spin.txt r6.byte \
                                                  spin.txt r1.byte 1 spin.txt r1.byte 4 spin.txt r1.byte 2)" 0
check "waiting jobs start by priority: pids 6, 7, 5 (pid:priority at start)" "$(starts)" "1:0 2:0 3:0 4:0 6:6 7:5 5:5"
check "all 7 jobs finished" "$(grep -c 'finished on processor' "$RUN/run.txt")" 7

# B: pid 5 (priority 0) waits from the start; pid 6 (priority 1) is typed in the shell after round 1.
printf 'spin.txt r1.byte 1\n' > "$RUN/cmds.txt"
check "scenario B runs without error" "$(run_sim spin.txt r4.byte spin.txt r6.byte spin.txt r6.byte spin.txt r6.byte \
                                                  spin.txt r1.byte < "$RUN/cmds.txt")" 0
check "aging: pid 5 catches up with pid 6 and, older, starts first" "$(starts)" "1:0 2:0 3:0 4:0 5:4 6:5"
check "a program without its data file is refused" "$(run_sim -b spin.txt r1.byte 1 spin.txt)" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
