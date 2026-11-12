#!/usr/bin/env bash
# Q090 checks: the shell command "run <program> <data> <n>" starts n copies of a task.
. "$(dirname "$0")/../lib.sh"
build_solution

# Shell input from a file: every line is read in the first shell() call, so the run is deterministic.
printf '%s\n' 'run ticker.txt ticker_data.byte 6' 'sum_array.txt sum_array_data.byte' \
              'run ticker.txt ticker_data.byte 0' 'run nothere.txt ticker_data.byte 3' 'run ticker.txt' 'exit' > "$RUN/cmds.txt"
check "the shell session runs without error" "$(run_sim < "$RUN/cmds.txt")" 0
check "run ... 6 created 6 copies" "$(grep -c 'created: ticker.txt ticker_data.byte' "$RUN/run.txt")" 6
check "4 copies loaded at once, pids 5 and 6 waited (and the sum_array task, pid 7)" \
      "$(grep -c 'is waiting for a free processor' "$RUN/run.txt")" 3
check "every copy has its own output (3 each)" \
      "$(for p in 1 2 3 4 5 6; do printf '%s ' "$(word_at "$RUN/ticker_data_$p.out" 0)"; done)" "3 3 3 3 3 3 "
check "copies 1-4 run side by side, then 5-6 (pids in log.txt)" \
      "$(awk '{ printf "%s%s", s, $3; s = " " } END { print "" }' "$RUN/log.txt")" "1 1 2 2 3 3 4 4 1 2 3 4 5 5 6 6 5 6"
check "a normal start in the same session still works (pid 7 = 45)" "$(word_at "$RUN/sum_array_data_7.out" 36)" 45
check "n = 0 and a missing n print the usage" "$(grep -c 'usage: run <program> <data.byte> <n>' "$RUN/run.txt")" 2
check "a missing program gives one error, not three" "$(grep -c 'cannot start task' "$RUN/run.txt")" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
