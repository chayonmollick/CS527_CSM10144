#!/usr/bin/env bash
# Q033 checks: `CMP xA xB` / `CMP xA c` (6D / 6E 0 A B) set the flags like SUB and write no register.
. "$(dirname "$0")/../lib.sh"
build_solution
check "cmp_max runs without error" "$(run_sim -b cmp_max.txt cmp_max_data.byte)" 0
OUT="$RUN/cmp_max_data_1.out"
check "max of 3 -8 12 7 -1 12 with CMP + BLE (signed)" "$(word_at "$OUT" 28)" 12
check "count-up loop with CMP x5 10 + BLT: 0 + ... + 9" "$(word_at "$OUT" 32)" 45
check "CMP wrote no register (x0 still 77)" "$(word_at "$OUT" 36)" 77
(cd "$RUN" && "$SIM" -c cmp_max.txt > listing.txt)
check "CMP x4 x3 encodes as 6D 00 04 03" "$(grep -c '6D 00 04 03   CMP x4 x3' "$RUN/listing.txt")" 1
check "CMP x5 10 encodes as 6E 00 05 0A" "$(grep -c '6E 00 05 0A   CMP x5 10' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
