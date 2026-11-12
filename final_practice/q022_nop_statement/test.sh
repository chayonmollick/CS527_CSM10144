#!/usr/bin/env bash
# Q022 checks: `nop` compiles to 07 00 00 00 (x0 = x0): it runs but changes no register and no flag.
. "$(dirname "$0")/../lib.sh"
build_solution
check "nop_loop runs without error" "$(run_sim -b nop_loop.txt nop_loop_data.byte)" 0
OUT="$RUN/nop_loop_data_1.out"
check "sum 1..10 with nops in the loop" "$(word_at "$OUT" 0)" 55
check "nops between the compare and BNE kept the flags (x1 = 10)" "$(word_at "$OUT" 4)" 10
check "nop (x0 = x0) keeps x0" "$(word_at "$OUT" 8)" 7
check "nops take an instruction slot: 3 + 10*7 + 3 + halt = 77" "$(grep -c 'after 77 instructions' "$RUN/run.txt")" 1
check "each nop line ran 10 times (.freq)" "$(awk -F'\t' '$2 ~ /^nop/ && $1 == 10 { n++ } END { print n + 0 }' "$RUN/nop_loop_1.freq")" 3
(cd "$RUN" && "$SIM" -c nop_loop.txt > listing.txt)
check "nop encodes as 07 00 00 00 and is listed as nop" "$(grep -c '07 00 00 00   nop' "$RUN/listing.txt")" 3
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
