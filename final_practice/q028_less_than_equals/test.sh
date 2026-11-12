#!/usr/bin/env bash
# Q028 checks: `xD = xA < xB` / `xD = xA == xB` give 1 or 0 (66 / 67 D A B); tokenize splits < and ==.
. "$(dirname "$0")/../lib.sh"
build_solution
check "count_cmp runs without error" "$(run_sim -b count_cmp.txt count_cmp_data.byte)" 0
OUT="$RUN/count_cmp_data_1.out"
check "how many of 3 5 -2 8 5 10 are < 5 (signed)" "$(word_at "$OUT" 32)" 2
check "how many are == 5" "$(word_at "$OUT" 36)" 2
check "5 < 5 is 0 (strict)" "$(word_at "$OUT" 40)" 0
check "5 == 5 is 1, written without spaces" "$(word_at "$OUT" 44)" 1
(cd "$RUN" && "$SIM" -c count_cmp.txt > listing.txt)
check "x7 = x6 < x2 encodes as 66 07 06 02" "$(grep -c '66 07 06 02   x7 = x6 < x2' "$RUN/listing.txt")" 1
check "x7 = x6 == x2 encodes as 67 07 06 02" "$(grep -c '67 07 06 02   x7 = x6 == x2' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
run_sim -b fir.txt fir_data.byte > /dev/null
check "the new tokenizer still reads the vector program fir (out[0] = 204)" "$(word_at "$RUN/fir_data_1.out" 256)" 204
finish
