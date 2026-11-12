#!/usr/bin/env bash
# Q047 checks: data words read and written per task (a vector access counts 8 words).
. "$(dirname "$0")/../lib.sh"
build_solution
mem() { grep -F "[mem] pid $1 " "$RUN/run.txt"; }
check "sum_array, array_add and pages run together" "$(run_sim -b sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte pages.txt pages_data.byte)" 0
check "sum_array: size + 9 elements read, the sum written" "$(mem 1)" "[mem] pid 1 (sum_array): 10 data word(s) read, 1 written"
check "array_add: N + 4 vector loads x 8 read, 2 vector stores x 8 written" "$(mem 2)" "[mem] pid 2 (array_add): 33 data word(s) read, 16 written"
check "pages: 3 loads, 4 stores (instruction fetches are not counted)" "$(mem 3)" "[mem] pid 3 (pages): 3 data word(s) read, 4 written"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
