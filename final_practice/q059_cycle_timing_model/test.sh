#!/usr/bin/env bash
# Q059 checks: cycles = 1 per instruction + 2 per data word + 100 per page fault, per task.
. "$(dirname "$0")/../lib.sh"
build_solution
cyc() { grep -F "[time] pid $1 " "$RUN/run.txt"; }
check "sum_array, array_add and pages run together" "$(run_sim -b sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte pages.txt pages_data.byte)" 0
check "sum_array: 71 + 2 x 11 = 93" "$(cyc 1)" "[time] pid 1 (sum_array): 93 cycles = 1 x 71 instructions + 2 x 11 memory words + 100 x 0 faults"
check "array_add: 31 + 2 x (33 + 16) = 129" "$(cyc 2)" "[time] pid 2 (array_add): 129 cycles = 1 x 31 instructions + 2 x 49 memory words + 100 x 0 faults"
check "pages: 15 + 2 x 7 + 100 x 3 = 329" "$(cyc 3)" "[time] pid 3 (pages): 329 cycles = 1 x 15 instructions + 2 x 7 memory words + 100 x 3 faults"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
