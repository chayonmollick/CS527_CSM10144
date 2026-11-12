#!/usr/bin/env bash
# Q060 checks: each task reports its most executed instruction (index, count, disassembly, source line).
. "$(dirname "$0")/../lib.sh"
build_solution
hot() { grep -F "[hot] pid $1 " "$RUN/run.txt"; }
check "countdown, sum_array and array_add run together" "$(run_sim -b countdown.txt countdown_data.byte sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte)" 0
check "countdown: the BEQ at .loop runs 6 times (the body 5)" "$(hot 1)" "[hot] pid 1 (countdown): instruction 3 (BEQ +4) executed 6 times, line 6: BEQ .done"
check "countdown still computes 5 + 4 + 3 + 2 + 1" "$(word_at "$RUN/countdown_data_1.out" 0)" 15
check "sum_array: compare and BEQ tie at 10; the first wins" "$(hot 2 | cut -d, -f1)" "[hot] pid 2 (sum_array): instruction 4 (x15 = x1 - x4) executed 10 times"
check "array_add: the loop test (3 times) beats the body (2)" "$(hot 3 | cut -d, -f1)" "[hot] pid 3 (array_add): instruction 6 (x7 = x6 - x1) executed 3 times"
(cd "$RUN" && "$SIM" -c countdown.txt > /dev/null)
check "a precompiled .byte program has no source line" "$(run_sim -b program.byte countdown_data.byte),$(hot 1)" "0,[hot] pid 1 (program): instruction 3 (BEQ +4) executed 6 times, no source line"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
finish
