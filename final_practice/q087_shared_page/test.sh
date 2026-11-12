#!/usr/bin/env bash
# Q087 checks: data page 7 (addresses 3584..4095) of every task is one shared frame.
. "$(dirname "$0")/../lib.sh"
line_of() { grep -n "$1" "$RUN/run.txt" | head -1 | cut -d: -f1; }
build_solution

check "reader (pid 1) + writer (pid 2) run without error" "$(run_sim -b reader.txt shm_data.byte writer.txt shm_data.byte)" 0
check "the reader added the writer's shared words (10 + 20 + 30)" "$(word_at "$RUN/shm_data_1.out" 0)" 60
check "the writer's private word 0 is its own (7)" "$(word_at "$RUN/shm_data_2.out" 0)" 7
check "the writer's output shows the shared page: word at 3584" "$(word_at "$RUN/shm_data_2.out" 3584)" 10
check "flag word at 3596" "$(word_at "$RUN/shm_data_2.out" 3596)" 1
check "the writer finished (and its frames were freed) before the reader" \
      "$([ "$(line_of 'pid 2 (writer) finished')" -lt "$(line_of 'pid 1 (reader) finished')" ] && echo yes)" yes
check "the shared frame is reserved at start-up (12 frames left, not 13)" \
      "$(grep -c 'pid 1 (reader) loaded on processor 0: 1 instruction + 1 data page(s), 12 frame(s) left' "$RUN/run.txt")" 1
check "page 7 is already mapped: the writer has no page fault" "$(grep -c 'pid 2 (writer) .* 0 page fault(s)' "$RUN/run.txt")" 1
check "the reader faults only on its private page 1" "$(grep -c 'pid 1 (reader) .* 1 page fault(s)' "$RUN/run.txt")" 1

awk 'BEGIN { for (i = 0; i < 897; i++) print "00 00 00 00" }' > "$RUN/big_data.byte"     # 3588 bytes
check "a data file that reaches the shared page is refused" "$(run_sim -b writer.txt big_data.byte)" 1
check "with a message" "$(grep -c 'reaches the shared page' "$RUN/run.txt")" 1

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
