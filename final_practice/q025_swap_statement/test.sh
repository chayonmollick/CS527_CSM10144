#!/usr/bin/env bash
# Q025 checks: `swap xA xB` becomes 3 instructions; labels after it, the .freq count and overflow are right.
. "$(dirname "$0")/../lib.sh"
build_solution
check "swap_sort runs without error" "$(run_sim -b swap_sort.txt swap_sort_data.byte)" 0
OUT="$RUN/swap_sort_data_1.out"
check "sorted pair: the smaller first" "$(word_at "$OUT" 8)" 4
check "sorted pair: the larger second" "$(word_at "$OUT" 12)" 9
check "Fibonacci loop with swap: F(10)" "$(word_at "$OUT" 16)" 55
check "Fibonacci loop with swap: F(11)" "$(word_at "$OUT" 20)" 89
check "swap works when xA + xB overflows (x7)" "$(word_at "$OUT" 32)" 100
check "swap works when xA + xB overflows (x8)" "$(word_at "$OUT" 36)" 2147483647
check ".freq counts the swap line once per run of it" "$(awk -F'\t' '$2 == "swap x4 x5" { print $1 }' "$RUN/swap_sort_1.freq")" 10
(cd "$RUN" && "$SIM" -c swap_sort.txt > listing.txt)
check "swap x4 x5 is 3 instructions in the listing" "$(grep -c '| swap x4 x5' "$RUN/listing.txt")" 3
check "they are add, sub, sub" "$(grep '| swap x4 x5' "$RUN/listing.txt" | cut -c6-16 | paste -sd'|' -)" "01 04 04 05|02 05 04 05|02 04 04 05"
check "BLE .sorted jumps over all 3 (offset +4)" "$(grep -c '1D 00 00 04   BLE +4' "$RUN/listing.txt")" 1
check "BNE .fib after a swap still goes back to .fib (offset -5)" "$(grep -c '11 00 00 FB   BNE -5' "$RUN/listing.txt")" 1
echo "swap x3 x3" > "$RUN/swap_same.txt"
check "swap of a register with itself is rejected" "$( (cd "$RUN" && "$SIM" -c swap_same.txt 2>&1) | grep -c 'two different')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
