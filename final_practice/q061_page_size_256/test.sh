#!/usr/bin/env bash
# Q061 checks: PAGESIZE 256 -> 4 + 16 = 20 pages per task, 32 frames (31 usable); results unchanged.
. "$(dirname "$0")/../lib.sh"
build_solution
# a 73-instruction program (292 bytes of code): 2 instruction pages of 256 bytes, but 1 page of 512
awk 'BEGIN { print "x1 = 0"; for (i = 0; i < 70; i++) print "x1 = x1 + 1"; print "[0] = x1" }' > "$RUN/long70.txt"
echo "00 00 00 00" > "$RUN/long70_data.byte"
check "4 tasks run without error" "$(run_sim -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte fir.txt fir_data.byte long70.txt long70_data.byte)" 0
loaded() { grep -c "pid $1 ($2) loaded on processor $3: $4 instruction + 1 data page(s), $5 frame(s) left" "$RUN/run.txt"; }
check "sum_array: 1 + 1 frames, 29 of 31 left" "$(loaded 1 sum_array 0 1 29)" 1
check "pages: 1 + 1 frames, 27 left" "$(loaded 2 pages 1 1 27)" 1
check "fir: 1 + 1 frames, 25 left" "$(loaded 3 fir 2 1 25)" 1
check "long70: 292 bytes of code = 2 instruction pages, 22 left" "$(loaded 4 long70 3 2 22)" 1
check "pages.txt: still 3 page faults (now data pages 3, 7, 14)" "$(grep -c 'pid 2 (pages) finished .* 3 page fault(s)' "$RUN/run.txt")" 1
check "fir: output at 256 is now data page 1 -> 1 page fault" "$(grep -c 'pid 3 (fir) finished .* 1 page fault(s)' "$RUN/run.txt")" 1
check "pages.txt result 7 + 7 + 7" "$(word_at "$RUN/pages_data_2.out" 0)" 21
check "fir out[0] = 1*1 + 2*2 + ... + 8*8" "$(word_at "$RUN/fir_data_3.out" 256)" 204
check "long70 counted to 70" "$(word_at "$RUN/long70_data_4.out" 0)" 70
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
