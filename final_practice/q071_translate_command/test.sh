#!/usr/bin/env bash
# Q071 checks: shell command "translate <pid> i|d <address>" reads the page table and prints the physical address.
. "$(dirname "$0")/../lib.sh"
build_solution
# touch_wait = pid 1 on processor 0 (PTBR 0): code -> frame 1, data page 0 -> frame 2; in round 1 it maps data
# pages 1 -> 3 and 3 -> 4, then loops for 60000 instructions. The shell reads the commands after round 1.
(cd "$RUN" && printf 'translate 1 i 8\ntranslate 1 d 0\ntranslate 1 d 1020\ntranslate 1 d 2040\ntranslate 1 d 3000\ntranslate 1 i 600\ntranslate 2 d 0\ntranslate 1 d 4096\nexit\n' \
    | "$SIM" touch_wait.txt touch_wait_data.byte > run.txt 2>&1)
has() { grep -c "$1" "$RUN/run.txt"; }
check "i 8: code page 0 = frame 1 -> 512 + 8" "$(has 'pid 1 instruction address 8: page 0, entry memory\[0\] = frame 1 -> physical address 520$')" 1
check "d 0: data page 0 = entry 2 = frame 2 -> 1024" "$(has 'pid 1 data address 0: page 0, entry memory\[2\] = frame 2 -> physical address 1024$')" 1
check "d 1020: page 1 = frame 3 -> 1536 + 508" "$(has 'pid 1 data address 1020: page 1, entry memory\[3\] = frame 3 -> physical address 2044$')" 1
check "d 2040: page 3 = frame 4 -> 2048 + 504" "$(has 'pid 1 data address 2040: page 3, entry memory\[5\] = frame 4 -> physical address 2552$')" 1
check "d 3000: page 5 was never touched" "$(has 'pid 1 data address 3000: page 5, entry memory\[7\] = 0 -> not mapped$')" 1
check "i 600: the program has no instruction page 1" "$(has 'pid 1 instruction address 600: page 1, entry memory\[1\] = 0 -> not mapped$')" 1
check "pid 2 does not exist" "$(has 'pid 2 is not running$')" 1
check "4096 is outside the data space" "$(has 'usage: translate <pid> i|d <address>')" 1
check "translate changed nothing: still 2 page faults, no error" \
    "$(has 'pid 1 (touch_wait) finished on processor 0 after 60011 instructions, 2 page fault(s)')" 1
check "touch_wait result" "$(word_at "$RUN/touch_wait_data_1.out" 0)" 20000
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
