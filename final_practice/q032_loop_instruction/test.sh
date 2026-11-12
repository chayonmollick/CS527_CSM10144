#!/usr/bin/env bash
# Q032 checks: `loop xR .label` (6C R 0 offset) decrements xR and branches while xR != 0.
. "$(dirname "$0")/../lib.sh"
build_solution
check "loop_sum runs without error" "$(run_sim -b loop_sum.txt loop_sum_data.byte)" 0
OUT="$RUN/loop_sum_data_1.out"
check "sum 10 + 9 + ... + 1 with loop" "$(word_at "$OUT" 4)" 55
check "the counter ends at 0" "$(word_at "$OUT" 8)" 0
check "nested loops: 3 x 4 iterations" "$(word_at "$OUT" 12)" 12
check "58 instructions ran (sub + BNE would need 83)" "$(grep -c 'after 58 instructions' "$RUN/run.txt")" 1
(cd "$RUN" && "$SIM" -c loop_sum.txt > listing.txt)
check "loop x1 .again encodes as 6C 01 00 FF" "$(grep -c '6C 01 00 FF   loop x1 -1' "$RUN/listing.txt")" 1
check "loop x3 .outer encodes as 6C 03 00 FD" "$(grep -c '6C 03 00 FD   loop x3 -3' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
