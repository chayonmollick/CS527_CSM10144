#!/usr/bin/env bash
# Q077 checks: -s fcfs runs one task at a time, each to completion, in arrival order; -s rr is the default.
. "$(dirname "$0")/../lib.sh"
build_solution
JOBS="counter_by100.txt counter_data.byte counter.txt counter_data.byte counter.txt counter_data.byte"   # 54, 43, 43 instructions
finish_order() { grep 'finished on processor' "$RUN/run.txt" | awk '{print $3}' | paste -sd, -; }
log_blocks() { awk '{print $3}' "$RUN/log.txt" | uniq | paste -sd, -; }
check "-s fcfs runs without error" "$(run_sim -b -s fcfs $JOBS)" 0
check "fcfs: pids 2 and 3 wait for their turn" "$(grep -c 'is waiting' "$RUN/run.txt")" 2
check "fcfs: every task is loaded on processor 0" "$(grep -c 'loaded on processor 0' "$RUN/run.txt")" 3
check "fcfs: the tasks finish in arrival order" "$(finish_order)" "1,2,3"
check "fcfs: log.txt has all of pid 1, then 2, then 3" "$(log_blocks)" "1,2,3"
check "fcfs: results unchanged (1000, 10, 10)" \
      "$(word_at "$RUN/counter_data_1.out" 0),$(word_at "$RUN/counter_data_2.out" 0),$(word_at "$RUN/counter_data_3.out" 0)" "1000,10,10"
run_sim -b -t -s fcfs $JOBS > /dev/null
check "fcfs -t: the trace is one unbroken block per task" \
      "$(grep '^\[pid' "$RUN/run.txt" | awk '{print $2}' | uniq -c | awk '{print $1 "x" $2}' | paste -sd, -)" "54x1,43x2,43x3"
: > "$RUN/log.txt"                                                  # log.txt is opened in append mode
run_sim -b $JOBS > /dev/null
check "default round robin: the short counters finish first" "$(finish_order)" "2,3,1"
check "default round robin: the turns interleave" "$(log_blocks | cut -d, -f1-6)" "1,2,3,1,2,3"
run_sim -b -s rr $JOBS > /dev/null
check "-s rr is the default" "$(finish_order)" "2,3,1"
check "-s fifo is rejected: usage, exit code 1" "$(run_sim -b -s fifo $JOBS),$(grep -c '^usage' "$RUN/run.txt")" "1,1"
(cd "$RUN" && "$SIM" -c counter.txt > listing.txt)
check "-c still prints the listing" "$(grep -c 'BNE -3' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
