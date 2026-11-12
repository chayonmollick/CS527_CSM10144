#!/usr/bin/env bash
# Q037 checks: printd prints in signed decimal; print and printd also write their line to the screen.
. "$(dirname "$0")/../lib.sh"
build_solution
check "printd runs without error" "$(run_sim -b printd.txt printd_data.byte)" 0
LOG="$RUN/log.txt"
check "print x1 is still hex in log.txt (000000C8)" "$(grep -cxF 'Process id: 1   x1 : 000000C8' "$LOG")" 1
check "printd x1 logs 200" "$(grep -cxF 'Process id: 1   x1 : 200' "$LOG")" 1
check "printd x2 logs -7 (signed)" "$(grep -cxF 'Process id: 1   x2 : -7' "$LOG")" 1
check "printd x3 logs 65025" "$(grep -cxF 'Process id: 1   x3 : 65025' "$LOG")" 1
check "print x2 logs FFFFFFF9" "$(grep -cxF 'Process id: 1   x2 : FFFFFFF9' "$LOG")" 1
check "the screen shows the same 5 lines in the same order" \
      "$(grep '^Process id' "$RUN/run.txt" | cmp -s - "$LOG" && echo same)" same
(cd "$RUN" && "$SIM" -c printd.txt > listing.txt)
check "printd x2 encodes as 88 00 00 02" "$(grep -cF '88 00 00 02   printd x2' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
