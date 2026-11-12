#!/usr/bin/env bash
# Q062 checks: finalize() prints the task's page table, read from frame 0 at PTBR[p], before freeing it.
. "$(dirname "$0")/../lib.sh"
build_solution
check "sum_array + pages run without error" "$(run_sim -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte)" 0
# loader: pid 1 gets frames 1 (code), 2 (data); pid 2 gets 3, 4; pages.txt then faults on data pages 1, 3, 7 -> frames 5, 6, 7
check "pid 1 table at PTBR 0 (processor 0)" \
    "$(grep -c '^\[os\] pid 1 page table (PTBR 0): I0=1 I1=- D0=2 D1=- D2=- D3=- D4=- D5=- D6=- D7=-$' "$RUN/run.txt")" 1
check "pid 2 table at PTBR 10, demand-mapped pages included" \
    "$(grep -c '^\[os\] pid 2 page table (PTBR 10): I0=3 I1=- D0=4 D1=5 D2=- D3=6 D4=- D5=- D6=- D7=7$' "$RUN/run.txt")" 1
check "the table is printed before the 'finished' line" \
    "$(grep -A1 'pid 2 page table' "$RUN/run.txt" | grep -c 'pid 2 (pages) finished')" 1
check "pages.txt result unchanged" "$(word_at "$RUN/pages_data_2.out" 0)" 21
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
