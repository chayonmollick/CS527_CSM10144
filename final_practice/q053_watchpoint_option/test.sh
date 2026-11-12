#!/usr/bin/env bash
# Q053 checks: -w ADDR prints every write to data address ADDR with the old and new value.
. "$(dirname "$0")/../lib.sh"
build_solution
watch() { grep '^\[watch\]' "$RUN/run.txt" | sed -n "$1p"; }   # watch <n>: the n-th [watch] line
check "-w 8 with watch.txt and sum_array" "$(run_sim -b -w 8 watch.txt watch_data.byte sum_array.txt sum_array_data.byte)" 0
check "4 writes to [8]" "$(grep -c '^\[watch\]' "$RUN/run.txt")" 4
check "1st: the loop, 7 -> 10" "$(watch 1)" "[watch] pid 1, instruction 3 ([8] = x2): [8] 7 -> 10"
check "2nd: 10 -> 20" "$(watch 2)" "[watch] pid 1, instruction 3 ([8] = x2): [8] 10 -> 20"
check "3rd: 20 -> 30" "$(watch 3)" "[watch] pid 1, instruction 3 ([8] = x2): [8] 20 -> 30"
check "4th: third word of the vector store, same value" "$(watch 4)" "[watch] pid 1, instruction 8 ([0] = v1): [8] 30 -> 30"
check "sum_array never writes [8]" "$(grep -c '^\[watch\] pid 2' "$RUN/run.txt")" 0
check "-w 36 on sum_array: the sum replaces the last element" "$(run_sim -b -w 36 sum_array.txt sum_array_data.byte),$(watch 1)" "0,[watch] pid 1, instruction 11 ([x3] = x2): [36] 9 -> 45"
check "-w 0x24 (hex) is the same address" "$(run_sim -b -w 0x24 sum_array.txt sum_array_data.byte),$(grep -c '\[36\] 9 -> 45' "$RUN/run.txt")" "0,1"
check "-w 4096 (outside the data space) is rejected" "$(run_sim -b -w 4096 sum_array.txt sum_array_data.byte),$(grep -c '^usage:' "$RUN/run.txt")" "1,1"
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "without -w: no [watch] lines, and sum_array = 45" "$(grep -c '^\[watch\]' "$RUN/run.txt"),$(word_at "$RUN/sum_array_data_1.out" 36)" "0,45"
finish
