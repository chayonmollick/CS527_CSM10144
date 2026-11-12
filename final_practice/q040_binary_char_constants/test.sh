#!/usr/bin/env bash
# Q040 checks: 0b binary, 0x hex and 'c' character constants, all still limited to 0..255.
. "$(dirname "$0")/../lib.sh"
build_solution
check "consts runs without error" "$(run_sim -b consts.txt consts_data.byte)" 0
OUT="$RUN/consts_data_1.out"
check "0b1010 = 10" "$(word_at "$OUT" 0)" 10
check "0xFF = 255" "$(word_at "$OUT" 4)" 255
check "'A' = 65" "$(word_at "$OUT" 8)" 65
check "x3 + 0b11 = 'D' = 68" "$(word_at "$OUT" 12)" 68
check "'7' - '0' = 7" "$(word_at "$OUT" 16)" 7
check "'%' + ',' = 37 + 44 = 81 (neither is special between quotes)" "$(word_at "$OUT" 20)" 81
check "' ' + 0B1 = 33" "$(word_at "$OUT" 24)" 33
(cd "$RUN" && "$SIM" -c consts.txt > listing.txt)
check "x1 = 0b1010 is MOVC 10: 0F 01 00 0A" "$(grep -cF "0F 01 00 0A   x1 = 10 " "$RUN/listing.txt")" 1
check "x3 = 'A' is MOVC 65: 0F 03 00 41" "$(grep -cF "0F 03 00 41   x3 = 65 " "$RUN/listing.txt")" 1
check "x6 = '%' compiles (the line was not cut at the %): 0F 06 00 25" "$(grep -cF "0F 06 00 25   x6 = 37 " "$RUN/listing.txt")" 1
check "0b100000000 (256) is out of range" \
      "$(cd "$RUN" && "$SIM" -c consts_bad.txt 2>&1 | grep -cF 'consts_bad.txt:2: constant 0b100000000 is out of range 0..255')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
