#!/usr/bin/env bash
# Q023 checks: `xD = -xA` is one new instruction (62 D 0 A); binary minus still compiles to SUB.
. "$(dirname "$0")/../lib.sh"
build_solution
check "negate runs without error" "$(run_sim -b negate.txt negate_data.byte)" 0
OUT="$RUN/negate_data_1.out"
check "-(25) = -25" "$(word_at "$OUT" 12)" -25
check "-(-7) = 7, in place" "$(word_at "$OUT" 16)" 7
check "-(-2147483648) wraps to -2147483648" "$(word_at "$OUT" 20)" -2147483648
check "binary minus still works: 25 - (-25) = 50" "$(word_at "$OUT" 24)" 50
check "unary minus leaves the flags alone (BEQ still taken)" "$(word_at "$OUT" 28)" 1
(cd "$RUN" && "$SIM" -c negate.txt > listing.txt)
check "x2 = -x1 encodes as 62 02 00 01" "$(grep -c '62 02 00 01   x2 = -x1' "$RUN/listing.txt")" 1
check "x6 = x1 - x2 is still SUB 02 06 01 02" "$(grep -c '02 06 01 02   x6 = x1 - x2' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
