#!/usr/bin/env bash
# Q076 checks: -q N sets the time slice at run time; -b, -t and -c still work.
. "$(dirname "$0")/../lib.sh"
build_solution
TWO="counter.txt counter_data.byte counter.txt counter_data.byte"
pids() { awk '{print $3}' "$RUN/log.txt" | paste -sd, -; }            # pid of every log.txt line
check "-q 4 runs without error" "$(run_sim -b -q 4 $TWO)" 0
check "-q 4: a slice is one loop turn (1 print), so the pids alternate" "$(pids)" "1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2,1,2"
check "-q 4: both counters still reach 10" "$(word_at "$RUN/counter_data_1.out" 0),$(word_at "$RUN/counter_data_2.out" 0)" "10,10"
: > "$RUN/log.txt"                                                  # log.txt is opened in append mode
run_sim -b -q 100 $TWO > /dev/null
check "-q 100: pid 1 prints all 10 values before pid 2 starts" "$(pids | tr -d ,)" "11111111112222222222"
: > "$RUN/log.txt"
run_sim -b $TWO > /dev/null
check "no -q: TIME_SLICE 10 as before (2 prints per first slice)" "$(pids | cut -d, -f1-4)" "1,1,2,2"
run_sim -b -t -q 4 $TWO > /dev/null
check "-t -q 4: the trace shows pid 1 in runs of 4 (43 = 10 x 4 + 3)" \
      "$(grep '^\[pid' "$RUN/run.txt" | awk '{print $2}' | uniq -c | awk '$2 == 1 {print $1}' | paste -sd, -)" "4,4,4,4,4,4,4,4,4,4,3"
check "-q 0 is rejected: usage, exit code 1" "$(run_sim -b -q 0 $TWO),$(grep -c '^usage' "$RUN/run.txt")" "1,1"
check "-q without a number is rejected" "$(run_sim -b -q),$(grep -c '^usage' "$RUN/run.txt")" "1,1"
(cd "$RUN" && "$SIM" -c counter.txt > listing.txt)
check "-c still prints the listing" "$(grep -c 'BNE -3' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
