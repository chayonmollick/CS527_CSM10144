#!/usr/bin/env bash
# Q039 checks: rol / ror by a constant or a register (amount mod 32); bits wrap round, none are lost.
. "$(dirname "$0")/../lib.sh"
build_solution
check "rotate runs without error" "$(run_sim -b rotate.txt rotate_data.byte)" 0
OUT="$RUN/rotate_data_1.out"
check "0x80000001 rol 3 = 0x0000000C (bit 31 wraps to bit 2)" "$(word_at "$OUT" 0)" 12
check "0xAB ror 4 = 0xB000000A (the low nibble wraps to the top)" "$(word_at "$OUT" 4)" -1342177270
check "0x12345678 (built with rol 8) ror x6, x6 = 8: 0x78123456" "$(word_at "$OUT" 8)" 2014458966
check "rol x6 with x6 = 36 rotates by 36 mod 32 = 4: 0x23456781" "$(word_at "$OUT" 12)" 591751041
check "ror x6 with x6 = 32 is a full turn: 0x12345678 again" "$(word_at "$OUT" 16)" 305419896
check "the .out file holds 78 56 34 12 at 16 (little-endian)" "$(sed -n 5p "$OUT")" "78 56 34 12"
(cd "$RUN" && "$SIM" -c rotate.txt > listing.txt)
check "x2 = x1 rol 3 encodes as 98 02 01 03 (constant form)" "$(grep -cF '98 02 01 03   x2 = x1 rol 3' "$RUN/listing.txt")" 1
check "x4 = x3 ror 4 encodes as 99 04 03 04" "$(grep -cF '99 04 03 04   x4 = x3 ror 4' "$RUN/listing.txt")" 1
check "x7 = x5 rol x6 encodes as 90 07 05 06 (register form)" "$(grep -cF '90 07 05 06   x7 = x5 rol x6' "$RUN/listing.txt")" 1
check "x7 = x5 ror x6 encodes as 91 07 05 06 (twice)" "$(grep -cF '91 07 05 06   x7 = x5 ror x6' "$RUN/listing.txt")" 2
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
