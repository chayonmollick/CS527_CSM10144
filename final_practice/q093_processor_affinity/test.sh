#!/usr/bin/env bash
# Q093 checks: "prog data @N" runs the task only on processor N; it waits while N is busy.
. "$(dirname "$0")/../lib.sh"
line_of() { grep -n "$1" "$RUN/run.txt" | head -1 | cut -d: -f1; }
build_solution

check "pid 1 @2, pid 2 @2, pid 3 free: runs without error" \
      "$(run_sim -b count3.txt count3_data.byte @2 count3.txt count3_data.byte @2 count3.txt count3_data.byte)" 0
check "pid 1 goes to processor 2 although 0 is free" "$(grep -c 'pid 1 (count3) loaded on processor 2' "$RUN/run.txt")" 1
check "pid 2 waits although processors 0, 1, 3 are free" "$(grep -c 'pid 2 is waiting' "$RUN/run.txt")" 1
check "pid 3 (no @) takes the first free processor, 0" "$(grep -c 'pid 3 (count3) loaded on processor 0' "$RUN/run.txt")" 1
check "pid 2 starts on processor 2 only after pid 1 has finished there" \
      "$([ "$(line_of 'pid 1 (count3) finished on processor 2')" -lt "$(line_of 'pid 2 (count3) loaded on processor 2')" ] && echo yes)" yes
check "pids in log.txt: 1 and 3 side by side, then 2" \
      "$(awk '{ printf "%s%s", s, $3; s = " " } END { print "" }' "$RUN/log.txt")" "1 1 3 3 1 3 2 2 2"
check "all three counted to 3" \
      "$(for p in 1 2 3; do printf '%s ' "$(word_at "$RUN/count3_data_$p.out" 0)"; done)" "3 3 3 "

check "@7 does not exist (NP = 4): refused" "$(run_sim -b count3.txt count3_data.byte @7)" 1
check "with a message" "$(grep -c "'@7' is not a processor @0..@3" "$RUN/run.txt")" 1
printf 'count3.txt count3_data.byte @3\n' > "$RUN/cmds.txt"
check "the shell accepts @N too" "$(run_sim < "$RUN/cmds.txt")" 0
check "and runs that task on processor 3" "$(grep -c 'pid 1 (count3) loaded on processor 3' "$RUN/run.txt")" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
