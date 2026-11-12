#!/usr/bin/env bash
# Q085 checks: sched.log has one line per time slice: round, pid, processor, instructions run.
. "$(dirname "$0")/../lib.sh"
build_solution
FIVE="counter.txt counter_data.byte counter_by100.txt counter_data.byte counter.txt counter_data.byte counter_by100.txt counter_data.byte counter.txt counter_data.byte"
SL="$RUN/sched.log"
slices() { awk 'NR > 1' "$SL" | wc -l | tr -d ' '; }
total() { awk 'NR > 1 {s += $4} END {print s}' "$SL"; }
in_round() { awk -v R="$1" 'NR > 1 && $1 == R {print $2 "@" $3 ":" $4}' "$SL" | paste -sd, -; }   # pid@processor:instructions
check "5 tasks on 4 processors run without error" "$(run_sim -b $FIVE)" 0
check "header line" "$(head -1 "$SL")" "round  pid  processor  instructions"
check "27 slices: rounds 1-5 4 each, round 6 3, rounds 7-10 1 each" "$(slices)" 27
check "every instruction is in one slice: 3 x 43 + 2 x 54 = 237" "$(total)" 237
check "per pid the sums match the [os] counts" \
      "$(awk 'NR > 1 {s[$2] += $4} END {for (p = 1; p <= 5; p++) printf "%s%d", (p > 1 ? "," : ""), s[p]}' "$SL")" "43,54,43,54,43"
check "round 1: pids 1-4 on processors 0-3, 10 each" "$(in_round 1)" "1@0:10,2@1:10,3@2:10,4@3:10"
check "round 5: the counters end with 3 instructions" "$(in_round 5)" "1@0:3,2@1:10,3@2:3,4@3:10"
check "round 6: pid 5 starts on processor 0, after pids 2 and 4" "$(in_round 6)" "2@1:4,4@3:4,5@0:10"
check "the last slice: round 10, pid 5, processor 0, 3 instructions" "$(tail -1 "$SL" | awk '{print $1, $2, $3, $4}')" "10 5 0 3"
run_sim -b $FIVE > /dev/null
check "a new run starts a new sched.log (not appended)" "$(slices)" 27
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
check "sum_array alone: 71 instructions in 8 slices" "$(slices),$(total)" "8,71"
finish
