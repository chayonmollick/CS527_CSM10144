#!/usr/bin/env bash
# Q021 checks: `xD = not xA` is one new instruction (61 D 0 A) that flips every bit and keeps the flags.
. "$(dirname "$0")/../lib.sh"
build_solution
check "not_bits runs without error" "$(run_sim -b not_bits.txt not_bits_data.byte)" 0
OUT="$RUN/not_bits_data_1.out"
check "not 0 = -1" "$(word_at "$OUT" 12)" -1
check "not 5 = -6 (not x = -x - 1)" "$(word_at "$OUT" 16)" -6
check "not 0x0F0F0F0F = 0xF0F0F0F0, in place" "$(word_at "$OUT" 20)" -252645136
check "not (not 5) = 5" "$(word_at "$OUT" 24)" 5
check "not leaves the flags alone (BEQ still taken)" "$(word_at "$OUT" 28)" 1
(cd "$RUN" && "$SIM" -c not_bits.txt > listing.txt)
check "x2 = not x1 encodes as 61 02 00 01" "$(grep -c '61 02 00 01   x2 = not x1' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
