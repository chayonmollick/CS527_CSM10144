#!/usr/bin/env bash
# Q041 checks: x1 op= y is compiled exactly like x1 = x1 op y (no new opcodes).
. "$(dirname "$0")/../lib.sh"
build_solution
check "compound runs without error" "$(run_sim -b compound.txt compound_data.byte)" 0
OUT="$RUN/compound_data_1.out"
check "((10 += 3) -= 5) *= 4, then /= 2 gives 16" "$(word_at "$OUT" 0)" 16
check "loop with += and -= (BNE uses the flags of -=) sums 10..1 = 55" "$(word_at "$OUT" 4)" 55
check "vector v1 *= 3, v1 += v1: lane 0 = 6 * 1" "$(word_at "$OUT" 64)" 6
check "vector lane 7 = 6 * 8 = 48" "$(word_at "$OUT" 92)" 48
(cd "$RUN" && "$SIM" -c compound.txt short.byte > listing.txt && "$SIM" -c compound_long.txt long.byte > /dev/null)
check "x1 += x2 is ADD: 01 01 01 02" "$(grep -F '| x1 += x2' "$RUN/listing.txt" | grep -cF '01 01 01 02   x1 = x1 + x2')" 1
check "x1 /= 2 is DIVI: 0C 01 01 02" "$(grep -F '| x1 /= 2' "$RUN/listing.txt" | grep -cF '0C 01 01 02   x1 = x1 / 2')" 1
check "byte code identical to the written-out program" "$(cmp -s "$RUN/short.byte" "$RUN/long.byte" && echo identical)" identical
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
