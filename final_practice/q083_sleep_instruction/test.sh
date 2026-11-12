#!/usr/bin/env bash
# Q083 checks: sleep n ends the current slice and skips the task's next n rounds.
. "$(dirname "$0")/../lib.sh"
build_solution
(cd "$RUN" && "$SIM" -c sleeper.txt > listing.txt)
check "sleep 2 encodes as E0 00 00 02" "$(grep -c 'E0 00 00 02   sleep 2' "$RUN/listing.txt")" 1
check "sleeper and counter run without error" "$(run_sim -b sleeper.txt counter_data.byte counter.txt counter_data.byte)" 0
check "log.txt: sleeper, counter 1-2 | 3-5 | 6-7 | sleeper, counter 8-10" \
      "$(awk '{print $3}' "$RUN/log.txt" | paste -sd, -)" "1,2,2,2,2,2,2,2,1,2,2,2"
check "the sleeper's second print is line 9, in round 4" "$(sed -n 9p "$RUN/log.txt")" "Process id: 1   x1 : 00000002"
check "the sleeper executed 7 instructions (the skipped rounds run none)" \
      "$(grep -c 'pid 1 (sleeper) finished on processor 0 after 7 instructions' "$RUN/run.txt")" 1
check "results: sleeper [0] = 2, counter [0] = 10" "$(word_at "$RUN/counter_data_1.out" 0),$(word_at "$RUN/counter_data_2.out" 0)" "2,10"
run_sim -b -t sleeper.txt counter_data.byte counter.txt counter_data.byte > /dev/null
check "trace: sleeper 3 (ends at sleep), counter 30 in a row (rounds 1-3), sleeper 4, counter 13" \
      "$(grep '^\[pid' "$RUN/run.txt" | awk '{print $2}' | uniq -c | awk '{print $1 "x" $2}' | paste -sd, -)" "3x1,30x2,4x1,13x2"
: > "$RUN/log.txt"                                                  # log.txt is opened in append mode
sed 's/^sleep 2 /sleep 0 /' "$RUN/sleeper.txt" > "$RUN/nap0.txt"     # same program with sleep 0
run_sim -b nap0.txt counter_data.byte counter.txt counter_data.byte > /dev/null
check "sleep 0 only ends the slice: the second print is first in round 2 (line 4)" \
      "$(grep -n 'Process id: 1   x1 : 00000002' "$RUN/log.txt" | cut -d: -f1)" 4
printf 'sleep x1\n' > "$RUN/bad.txt"
check "sleep needs a constant" "$(cd "$RUN" && "$SIM" -c bad.txt 2>&1 | grep -c 'usage: sleep <rounds 0..255>')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
