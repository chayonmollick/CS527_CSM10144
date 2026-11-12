#!/usr/bin/env bash
# Q031 checks: `JMP .label` (6B 0 0 index) goes to an absolute index and reaches labels a branch cannot.
. "$(dirname "$0")/../lib.sh"
build_solution
check "jmp_loop runs without error" "$(run_sim -b jmp_loop.txt jmp_loop_data.byte)" 0
OUT="$RUN/jmp_loop_data_1.out"
check "loop closed by JMP: 10 + 9 + ... + 1" "$(word_at "$OUT" 4)" 55
check "JMP .end skipped the store" "$(word_at "$OUT" 8)" 0
(cd "$RUN" && "$SIM" -c jmp_loop.txt > listing.txt)
check "JMP .loop holds the index 2, not an offset" "$(grep -c '6B 00 00 02   JMP 2' "$RUN/listing.txt")" 1
check "JMP .end holds the index 9 of the added halt" "$(grep -c '6B 00 00 09   JMP 9' "$RUN/listing.txt")" 1
# Jump over 200 instructions: too far for an 8-bit relative offset, fine for an absolute index.
{ echo "x1 = 1"; echo "JMP .far"; for _ in $(seq 200); do echo "x1 = x1 + 1"; done; echo ".far"; echo "[0] = x1"; } > "$RUN/far.txt"
echo "00 00 00 00" > "$RUN/far_data.byte"
check "far.txt runs without error" "$(run_sim -b far.txt far_data.byte)" 0
check "JMP .far is 6B 00 00 CA (index 202)" "$(sed -n 2p "$RUN/program.byte")" "6B 00 00 CA"
check "JMP skipped the 200 additions" "$(word_at "$RUN/far_data_1.out" 0)" 1
check "only 4 instructions ran" "$(grep -c 'after 4 instructions' "$RUN/run.txt")" 1
sed 's/JMP .far/BAL .far/' "$RUN/far.txt" > "$RUN/far_bal.txt"
check "the same jump with BAL is too far (offset 201 > 127)" "$( (cd "$RUN" && "$SIM" -c far_bal.txt 2>&1) | grep -c 'too far')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
