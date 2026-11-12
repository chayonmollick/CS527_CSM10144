#!/usr/bin/env bash
# Q034 checks: negative constants become the opposite operation (one instruction) or two instructions.
. "$(dirname "$0")/../lib.sh"
build_solution
check "negconst runs without error" "$(run_sim -b negconst.txt negconst_data.byte)" 0
OUT="$RUN/negconst_data_1.out"
check "x1 = x2 + -5 (x2 = 20) gives 15" "$(word_at "$OUT" 0)" 15
check "x1 = x2 - -3 gives 23" "$(word_at "$OUT" 4)" 23
check "x1 = -7 stores FF FF FF F9 (-7)" "$(word_at "$OUT" 8)" -7
check "x3 = x1 + -1 gives -8" "$(word_at "$OUT" 12)" -8
check "loop adding x6 = -2 three times gives -6" "$(word_at "$OUT" 16)" -6
check ".freq counts the 2-instruction line once per run (3, not 6)" \
      "$(awk -F'\t' '$2 ~ /^x6 = -2/ { print $1 }' "$RUN/negconst_1.freq")" 3
(cd "$RUN" && "$SIM" -c negconst.txt > listing.txt)
check "x2 + -5 compiles to one SUBI: 0A 01 02 05" "$(grep -c '0A 01 02 05   x1 = x2 - 5 ' "$RUN/listing.txt")" 1
check "x2 - -3 compiles to one ADDI: 09 01 02 03" "$(grep -c '09 01 02 03   x1 = x2 + 3 ' "$RUN/listing.txt")" 1
check "x1 = -7 is two instructions: MOVC 0, then SUBI 7" \
      "$(grep -cE '(0F 01 00 00   x1 = 0 |0A 01 01 07   x1 = x1 - 7 ).*\| x1 = -7' "$RUN/listing.txt")" 2
check "pass 1 counted them: 19 instructions, BNE -4 back to .loop" \
      "$(grep -c -e '(19 instructions)' -e '11 00 00 FC   BNE -4' "$RUN/listing.txt")" 2
check "x2 = x1 * -3 is a compile error" \
      "$(cd "$RUN" && "$SIM" -c negconst_bad.txt 2>&1 | grep -c 'negconst_bad.txt:3: a negative constant works only with + and -')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
