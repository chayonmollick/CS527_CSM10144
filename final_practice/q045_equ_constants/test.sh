#!/usr/bin/env bash
# Q045 checks: .equ NAME value names a constant 0..255, usable wherever a constant is allowed.
. "$(dirname "$0")/../lib.sh"
build_solution
check "equ runs without error" "$(run_sim -b equ.txt equ_data.byte)" 0
check "sum of N = 5 elements from FIRST, stored at [RESULT] (0x40 = 64): 35" "$(word_at "$RUN/equ_data_1.out" 64)" 35
check "x5 = [FIRST] * N, stored with Write x5 SLOT (SLOT = 68, defined at the end): 15" "$(word_at "$RUN/equ_data_1.out" 68)" 15
(cd "$RUN" && "$SIM" -c equ.txt > listing.txt)
check ".equ lines give no instructions: 13 instructions, BNE -4" \
      "$(grep -c -e '(13 instructions)' -e '11 00 00 FC   BNE -4' "$RUN/listing.txt")" 2
check "x3 = FIRST compiles to MOVC 4" "$(grep -F '| x3 = FIRST' "$RUN/listing.txt" | grep -cF '0F 03 00 04   x3 = 4')" 1
check "[RESULT] = x2 compiles to [64] = x2" "$(grep -F '| [RESULT] = x2' "$RUN/listing.txt" | grep -cF '0E 40 00 02   [64] = x2')" 1
check ".freq: no .equ lines, the label .loop is still there" \
      "$(grep -c '^\.equ' "$RUN/equ_1.freq") $(grep -c '^\.loop' "$RUN/equ_1.freq")" "0 1"
check ".equ BIG 300 is a compile error" \
      "$(cd "$RUN" && "$SIM" -c equ_bad_value.txt 2>&1 | grep -cF 'equ_bad_value.txt:2: the value of BIG must be a constant 0..255')" 1
check ".equ x1 5 is a compile error (x1 is a register)" \
      "$(cd "$RUN" && "$SIM" -c equ_bad_name.txt 2>&1 | grep -cF 'equ_bad_name.txt:2: usage: .equ NAME value')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
