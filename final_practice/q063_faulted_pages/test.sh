#!/usr/bin/env bash
# Q063 checks: each finished task lists the data pages that page-faulted, in the order they faulted.
. "$(dirname "$0")/../lib.sh"
build_solution
check "sum_array + pages + scatter run without error" \
    "$(run_sim -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte scatter.txt scatter_data.byte)" 0
check "sum_array touched only its data file: none" "$(grep -c '^\[os\] pid 1 page faults on data pages: none$' "$RUN/run.txt")" 1
check "pages.txt: 1020, 2040, 3825 -> pages 1 3 7" "$(grep -c '^\[os\] pid 2 page faults on data pages: 1 3 7$' "$RUN/run.txt")" 1
check "scatter: fault order 5 2 6, then 3 4 from one store" "$(grep -c '^\[os\] pid 3 page faults on data pages: 5 2 6 3 4$' "$RUN/run.txt")" 1
check "the count still agrees (5 page faults)" "$(grep -c 'pid 3 (scatter) finished .* 5 page fault(s)' "$RUN/run.txt")" 1
check "scatter read back 1 + 2 + 3 + 4" "$(word_at "$RUN/scatter_data_3.out" 0)" 10
check "the word across pages 3/4 is intact" "$(word_at "$RUN/scatter_data_3.out" 2046)" 4
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
