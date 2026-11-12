#!/usr/bin/env bash
# Q000 checks: `halt` compiles to opcode 00 and stops the program early.
. "$(dirname "$0")/../lib.sh"
build_solution
check "early_stop runs without error" "$(run_sim -b early_stop.txt early_stop_data.byte)" 0
check "counter stored when it reached 5" "$(word_at "$RUN/early_stop_data_1.out" 0)" 5
check "code after halt never ran" "$(word_at "$RUN/early_stop_data_1.out" 4)" 0
(cd "$RUN" && "$SIM" -c early_stop.txt > listing.txt)
check "halt and the added end are both 00 00 00 00" "$(grep -c '00 00 00 00   halt' "$RUN/listing.txt")" 2
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
