#!/usr/bin/env bash
# Q048 checks: `xD = steps` (opcode C0) reads the task's own instruction counter.
. "$(dirname "$0")/../lib.sh"
build_solution
(cd "$RUN" && "$SIM" -c steps.txt > listing.txt)
check "x1 = steps compiles to C0 01 00 00" "$(grep -c 'C0 01 00 00   x1 = steps' "$RUN/listing.txt")" 1
check "two copies run at the same time" "$(run_sim -b steps.txt steps_data.byte steps.txt steps_data.byte)" 0
check "the first instruction reads 1 (it counts itself)" "$(word_at "$RUN/steps_data_1.out" 0)" 1
check "after the loop: 2 + 5 x 3 + 1 = 18" "$(word_at "$RUN/steps_data_1.out" 4)" 18
check "17 instructions from the first read to the second" "$(word_at "$RUN/steps_data_1.out" 8)" 17
check "pid 2 has its own count (round robin does not mix them)" "$(word_at "$RUN/steps_data_2.out" 4)" 18
check "each task ran 23 instructions" "$(grep -c 'after 23 instructions' "$RUN/run.txt")" 2
printf 'v1 = steps\n' > "$RUN/vsteps.txt"
(cd "$RUN" && "$SIM" -c vsteps.txt > listing.txt 2>&1)
check "a vector destination is a compile error" "$(grep -c 'usage: x<n> = steps' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
