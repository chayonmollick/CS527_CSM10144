#!/usr/bin/env bash
# Q027 checks: `xD = min xA xB` / `xD = max xA xB` (64 / 65 D A B) compare as signed numbers.
. "$(dirname "$0")/../lib.sh"
build_solution
check "min_max runs without error" "$(run_sim -b min_max.txt min_max_data.byte)" 0
OUT="$RUN/min_max_data_1.out"
check "min of 5 -3 12 -20 7 0" "$(word_at "$OUT" 28)" -20
check "max of 5 -3 12 -20 7 0" "$(word_at "$OUT" 32)" 12
check "signed: min(-3, 1) = -3" "$(word_at "$OUT" 36)" -3
(cd "$RUN" && "$SIM" -c min_max.txt > listing.txt)
check "x3 = min x3 x5 encodes as 64 03 03 05" "$(grep -c '64 03 03 05   x3 = min x3 x5' "$RUN/listing.txt")" 1
check "x4 = max x4 x5 encodes as 65 04 04 05" "$(grep -c '65 04 04 05   x4 = max x4 x5' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
