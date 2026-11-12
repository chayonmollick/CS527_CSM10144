#!/usr/bin/env bash
# Q030 checks: `BZ xR .label` / `BNZ xR .label` (69 / 6A R 0 offset) test the register, not the flags.
. "$(dirname "$0")/../lib.sh"
build_solution
check "bz_count runs without error" "$(run_sim -b bz_count.txt bz_count_data.byte)" 0
OUT="$RUN/bz_count_data_1.out"
check "zeros in 7 0 3 0 9 counted with BNZ (the flags said non-zero)" "$(word_at "$OUT" 24)" 2
check "BZ x5 jumped although Z = 0: the store was skipped" "$(word_at "$OUT" 28)" 0
check "the code after the label ran" "$(word_at "$OUT" 32)" 1
(cd "$RUN" && "$SIM" -c bz_count.txt > listing.txt)
check "BNZ x4 .nonzero encodes as 6A 04 00 02" "$(grep -c '6A 04 00 02   BNZ x4 +2' "$RUN/listing.txt")" 1
check "BNZ x1 .next (backwards) encodes as 6A 01 00 FB" "$(grep -c '6A 01 00 FB   BNZ x1 -5' "$RUN/listing.txt")" 1
check "BZ x5 .skip encodes as 69 05 00 02" "$(grep -c '69 05 00 02   BZ x5 +2' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
