#!/usr/bin/env bash
# Q086 checks: semaphores wait/signal/sem; a blocked task executes nothing until it is signalled.
. "$(dirname "$0")/../lib.sh"
log_pids()   { awk '{ printf "%s%s", s, $3; s = " " } END { print "" }' "$RUN/log.txt"; }
log_values() { awk 'function hex(h,  i, v) { for (i = 1; i <= length(h); i++) v = v * 16 + index("0123456789ABCDEF", substr(h, i, 1)) - 1; return v }
                    { printf "%s%d", s, hex($NF); s = " " } END { print "" }' "$RUN/log.txt"; }
build_solution

(cd "$RUN" && "$SIM" -c ping.txt > listing.txt)
check "sem 1 0 encodes as EA 01 00 00" "$(grep -c 'EA 01 00 00   sem 1 0' "$RUN/listing.txt")" 1
check "wait 0 encodes as E8 00 00 00" "$(grep -c 'E8 00 00 00   wait 0' "$RUN/listing.txt")" 1
check "signal 1 encodes as E9 01 00 00" "$(grep -c 'E9 01 00 00   signal 1' "$RUN/listing.txt")" 1
printf 'wait 8\n' > "$RUN/bad_sem.txt"
check "wait 8 is a compile error (only 0..7)" "$(cd "$RUN" && "$SIM" -c bad_sem.txt > /dev/null 2>&1; echo $?)" 1

rm -f "${RUN:?}/log.txt"
check "ping + pong run without error" "$(run_sim -b ping.txt sem_data.byte pong.txt sem_data.byte)" 0
check "the two tasks take turns (pids in log.txt)" "$(log_pids)" "1 2 1 2 1 2 1 2 1 2"
check "printed values alternate" "$(log_values)" "1 101 2 102 3 103 4 104 5 105"
check "ping stored 5" "$(word_at "$RUN/sem_data_1.out" 0)" 5
check "pong stored 105" "$(word_at "$RUN/sem_data_2.out" 0)" 105
check "blocked ping executed nothing extra (34 instructions)" \
      "$(grep -c 'pid 1 (ping) finished on processor 0 after 34 instructions' "$RUN/run.txt")" 1
check "blocked pong executed nothing extra (33 instructions)" \
      "$(grep -c 'pid 2 (pong) finished on processor 1 after 33 instructions' "$RUN/run.txt")" 1

rm -f "${RUN:?}/log.txt"
check "3 copies of mutex run without error" "$(run_sim -b mutex.txt sem_data.byte mutex.txt sem_data.byte mutex.txt sem_data.byte)" 0
check "critical sections do not interleave, woken in FIFO order" "$(log_pids)" "1 1 1 2 2 2 3 3 3"
check "each copy executed wait 0 once (no busy waiting)" "$(grep -c $'^1\twait 0' "$RUN/mutex_3.freq")" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
