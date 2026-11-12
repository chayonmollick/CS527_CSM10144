#!/usr/bin/env bash
# Q035 checks: x1 += x2 * x3 is one instruction (83 D A B) that also reads its destination.
. "$(dirname "$0")/../lib.sh"
build_solution
check "dot_mac runs without error" "$(run_sim -b dot_mac.txt dot_mac_data.byte)" 0
check "dot product 1*5 + (-2)*6 + 3*7 + 4*8 = 46" "$(word_at "$RUN/dot_mac_data_1.out" 36)" 46
check "x7 += x7 * x7 (x7 = 3) gives 12, and BEQ still saw Z = 1" "$(word_at "$RUN/dot_mac_data_1.out" 40)" 12
check "41 instructions executed (7 per element, not 8)" "$(grep -c 'after 41 instructions' "$RUN/run.txt")" 1
(cd "$RUN" && "$SIM" -c dot_mac.txt > listing.txt)
check "x4 += x5 * x6 is one instruction: 83 04 05 06" "$(grep -cF '83 04 05 06   x4 += x5 * x6' "$RUN/listing.txt")" 1
check "x7 += x7 * x7 encodes as 83 07 07 07" "$(grep -c '83 07 07 07' "$RUN/listing.txt")" 1
check "x1 += x1 * 3 is a compile error" \
      "$(cd "$RUN" && "$SIM" -c mac_bad.txt 2>&1 | grep -cF 'mac_bad.txt:3: usage: xD += xA * xB')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
