#!/usr/bin/env bash
# Q036 checks: xD = [xA]+ and [xA]+ = xS access memory at xA, then add 4 to xA.
. "$(dirname "$0")/../lib.sh"
build_solution
check "copy_sum runs without error" "$(run_sim -b copy_sum.txt copy_sum_data.byte)" 0
OUT="$RUN/copy_sum_data_1.out"
check "copy: B[0] = 10 at 64" "$(word_at "$OUT" 64)" 10
check "copy: B[2] = 30 at 72" "$(word_at "$OUT" 72)" 30
check "copy: B[4] = 50 at 80" "$(word_at "$OUT" 80)" 50
check "sum 10+20+30+40+50 = 150 at 84 (the store pointer moved on by itself)" "$(word_at "$OUT" 84)" 150
check "source pointer after 5 loads: 4 + 5*4 = 24" "$(word_at "$OUT" 88)" 24
check "[x4]+ = x4 stores the old x4 (92), then increments" "$(word_at "$OUT" 92)" 92
check "x4 = 96 after the last post-increment store" "$(word_at "$OUT" 96)" 96
check "34 instructions executed (5 per element instead of 7)" "$(grep -c 'after 34 instructions' "$RUN/run.txt")" 1
(cd "$RUN" && "$SIM" -c copy_sum.txt > listing.txt)
check "x6 = [x3]+ encodes as 85 06 00 03" "$(grep -cF '85 06 00 03   x6 = [x3]+' "$RUN/listing.txt")" 1
check "[x4]+ = x6 encodes as 86 04 00 06" "$(grep -cF '86 04 00 06   [x4]+ = x6' "$RUN/listing.txt")" 1
check "x3 = [x3]+ is a compile error" \
      "$(cd "$RUN" && "$SIM" -c postinc_bad.txt 2>&1 | grep -cF 'postinc_bad.txt:3: xD = [xD]+ would load and increment the same register')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
