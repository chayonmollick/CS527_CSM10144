#!/usr/bin/env bash
# Q026 checks: `xD = abs xA` is one new instruction (63 D 0 A).
. "$(dirname "$0")/../lib.sh"
build_solution
check "abs_sum runs without error" "$(run_sim -b abs_sum.txt abs_sum_data.byte)" 0
OUT="$RUN/abs_sum_data_1.out"
check "sum of |3| |-4| |0| |-10| |6|" "$(word_at "$OUT" 24)" 23
check "abs in place: |-4| = 4" "$(word_at "$OUT" 28)" 4
check "abs(-2147483648) wraps to -2147483648" "$(word_at "$OUT" 36)" -2147483648
check "the array itself is unchanged (a[3] = -10)" "$(word_at "$OUT" 16)" -10
(cd "$RUN" && "$SIM" -c abs_sum.txt > listing.txt)
check "x5 = abs x4 encodes as 63 05 00 04" "$(grep -c '63 05 00 04   x5 = abs x4' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
