#!/usr/bin/env bash
# Q072 checks: ".readonly <start> <end>" makes data bytes read-only; a store into them is a protection fault.
. "$(dirname "$0")/../lib.sh"
build_solution
(cd "$RUN" && "$SIM" -c consts.txt > listing.txt)
check "the directive is not an instruction (8 statements + end = 9)" "$(grep -c '(9 instructions)' "$RUN/listing.txt")" 1
check "consts fails, sum_array runs: exit code 1" "$(run_sim -b consts.txt consts_data.byte sum_array.txt sum_array_data.byte)" 1
check "the store to 8 is a protection fault (instruction 6)" \
    "$(grep -c 'error at instruction 6: protection fault: store to 8..11 overlaps the read-only range 0..11' "$RUN/run.txt")" 1
check "the task stopped after 7 instructions" "$(grep -c 'pid 1 (consts) STOPPED WITH AN ERROR .* after 7 instructions' "$RUN/run.txt")" 1
check "reads of the constants worked: [12] = 10 + 20 + 30" "$(word_at "$RUN/consts_data_1.out" 12)" 60
check "the constant at 8 is unchanged" "$(word_at "$RUN/consts_data_1.out" 8)" 30
check "the line after the fault never ran" "$(word_at "$RUN/consts_data_1.out" 16)" 0
check "the other task is not affected" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
# a word that only partly overlaps the range (bytes 10..13 vs 0..11) is also refused
printf '.readonly 0 11\nx1 = 5\n[12] = x1\n[10] = x1\n' > "$RUN/edge.txt"
check "a store that overlaps the range by 2 bytes fails" "$(run_sim -b edge.txt consts_data.byte)" 1
check "with the fault message" "$(grep -c 'protection fault: store to 10..13 overlaps the read-only range 0..11' "$RUN/run.txt")" 1
printf '.readonly 20 10\nx1 = 1\n' > "$RUN/bad.txt"
(cd "$RUN" && "$SIM" -c bad.txt > run.txt 2>&1)
check "a bad range is a compile error" "$(grep -c '^bad.txt:1: usage: .readonly <start> <end>' "$RUN/run.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
