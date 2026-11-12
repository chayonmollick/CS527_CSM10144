#!/usr/bin/env bash
# Q024 checks: `x1++` / `x1--` compile to ADDI / SUBI with constant 1 (compiler only), so they set the flags.
. "$(dirname "$0")/../lib.sh"
build_solution
check "inc_dec runs without error" "$(run_sim -b inc_dec.txt inc_dec_data.byte)" 0
OUT="$RUN/inc_dec_data_1.out"
check "sum 10 + 9 + ... + 1 with x1-- and BNE" "$(word_at "$OUT" 4)" 55
check "x3++ counted 10 iterations" "$(word_at "$OUT" 8)" 10
check "x1-- set Z at 0, which ended the loop" "$(word_at "$OUT" 12)" 0
check "0-- = -1" "$(word_at "$OUT" 16)" -1
check "x4-- set N, so BMI was taken" "$(word_at "$OUT" 20)" 1
(cd "$RUN" && "$SIM" -c inc_dec.txt > listing.txt)
check "x3++ is ADDI 09 03 03 01 (same as x3 = x3 + 1)" "$(grep -c '09 03 03 01   x3 = x3 + 1 .*| x3++' "$RUN/listing.txt")" 1
check "x1-- is SUBI 0A 01 01 01 (same as x1 = x1 - 1)" "$(grep -c '0A 01 01 01   x1 = x1 - 1 .*| x1--' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
