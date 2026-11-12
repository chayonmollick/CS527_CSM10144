#!/usr/bin/env bash
# Q029 checks: `xD = xA if xB` (68 D A B) copies only when xB != 0, without a branch.
. "$(dirname "$0")/../lib.sh"
build_solution
check "cmov runs without error" "$(run_sim -b cmov.txt cmov_data.byte)" 0
OUT="$RUN/cmov_data_1.out"
check "last non-zero element of 4 0 9 -3 0 0" "$(word_at "$OUT" 28)" -3
check "how many are non-zero (conditional increment)" "$(word_at "$OUT" 32)" 3
check "condition 0: the destination keeps its value" "$(word_at "$OUT" 36)" 77
(cd "$RUN" && "$SIM" -c cmov.txt > listing.txt)
check "x3 = x5 if x5 encodes as 68 03 05 05" "$(grep -c '68 03 05 05   x3 = x5 if x5' "$RUN/listing.txt")" 1
check "x4 = x6 if x5 encodes as 68 04 06 05" "$(grep -c '68 04 06 05   x4 = x6 if x5' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
