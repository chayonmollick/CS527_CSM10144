#!/usr/bin/env bash
# Q057 checks: each task's highest data address and number of distinct words written.
. "$(dirname "$0")/../lib.sh"
build_solution
fp() { grep -F "[footprint] pid $1 " "$RUN/run.txt"; }
check "four tasks run together" "$(run_sim -b footprint.txt footprint_data.byte sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte pages.txt pages_data.byte)" 0
check "footprint: [8] five times + [100] + [4] = 3 words; a load of [200]" "$(fp 1)" "[footprint] pid 1 (footprint): highest data address 200, 3 distinct word(s) written"
check "sum_array: reads 0..36, writes only [36]" "$(fp 2)" "[footprint] pid 2 (sum_array): highest data address 36, 1 distinct word(s) written"
check "array_add: 2 vector stores x 8 words, C[15] at 192" "$(fp 3)" "[footprint] pid 3 (array_add): highest data address 192, 16 distinct word(s) written"
check "pages: 4 stores, the highest at 3825" "$(fp 4)" "[footprint] pid 4 (pages): highest data address 3825, 4 distinct word(s) written"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_2.out" 36)" 45
finish
