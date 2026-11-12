#!/usr/bin/env bash
# Q043 checks: x1 = vmax v2 / x1 = vmin v2 reduce the 8 lanes to their (signed) maximum / minimum.
. "$(dirname "$0")/../lib.sh"
build_solution
check "maxmin runs without error" "$(run_sim -b maxmin.txt maxmin_data.byte)" 0
OUT="$RUN/maxmin_data_1.out"
check "max of all 16 elements = 30" "$(word_at "$OUT" 64)" 30
check "min of all 16 elements = -25" "$(word_at "$OUT" 68)" -25
check "vmax of the first half = 12 (signed: not -3 = 0xFFFFFFFD)" "$(word_at "$OUT" 72)" 12
check "vmin of the first half = -20 (signed: not 0)" "$(word_at "$OUT" 76)" -20
check "vmax of an all-negative vector = -70 (starts from lane 0)" "$(word_at "$OUT" 80)" -70
(cd "$RUN" && "$SIM" -c maxmin.txt > listing.txt)
check "x1 = vmax v1 encodes as B0 01 00 01" "$(grep -cF 'B0 01 00 01   x1 = vmax v1' "$RUN/listing.txt")" 1
check "x3 = vmin v1 encodes as B1 03 00 01" "$(grep -cF 'B1 03 00 01   x3 = vmin v1' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
