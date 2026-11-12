#!/usr/bin/env bash
# Q084 checks: yield ends the current time slice early; the task continues after it next round.
. "$(dirname "$0")/../lib.sh"
build_solution
runs() { grep '^\[pid' "$RUN/run.txt" | awk '{print $2}' | uniq -c | awk -v P="$1" '$2 == P {print $1}' | paste -sd, -; }
(cd "$RUN" && "$SIM" -c polite.txt > listing.txt)
check "yield encodes as E1 00 00 00" "$(grep -c 'E1 00 00 00   yield' "$RUN/listing.txt")" 1
check "polite and counter run without error" "$(run_sim -b polite.txt counter_data.byte counter.txt counter_data.byte)" 0
check "log.txt: polite prints once per round, counter 2, 3, 2, 3 times" \
      "$(awk '{print $3}' "$RUN/log.txt" | paste -sd, -)" "1,2,2,1,2,2,2,1,2,2,1,2,2,2,1"
check "counter ends in round 5, polite in round 6" "$(grep 'finished on processor' "$RUN/run.txt" | awk '{print $3}' | paste -sd, -)" "2,1"
check "polite executed 28 instructions (1 + 5 x 5 + 2)" "$(grep -c 'pid 1 (polite) finished on processor 0 after 28 instructions' "$RUN/run.txt")" 1
check "results: polite [0] = 5, counter [0] = 10" "$(word_at "$RUN/counter_data_1.out" 0),$(word_at "$RUN/counter_data_2.out" 0)" "5,10"
run_sim -b -t polite.txt counter_data.byte counter.txt counter_data.byte > /dev/null
check "trace: polite's slices end at each yield (4,5,5,5,5 then 4 to the end)" "$(runs 1)" "4,5,5,5,5,4"
check "trace: counter's slices are still 10 (43 = 4 x 10 + 3)" "$(runs 2)" "10,10,10,10,3"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
