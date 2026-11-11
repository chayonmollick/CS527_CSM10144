#!/usr/bin/env bash
# e13 tests: demand paging with page replacement (backing store, FIFO/LRU, local/global).
#   ./test.sh          builds ./sim (make -s: 8 KB, local FIFO) and three variants in a scratch
#                      directory: 2 KB memory with FIFO and with LRU, and global replacement
# The expected numbers are derived by hand in README.md.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

build() {   # build <name> <-D flags>: a separate copy of the simulator in $WORK/<name>
    mkdir -p "$WORK/$1" && cp "$HERE"/*.cpp "$HERE"/*.h "$HERE"/Makefile "$WORK/$1"/ &&
        (cd "$WORK/$1" && make -s CXXFLAGS="-std=c++17 -O2 -Wall -Wextra $2") || { echo "build $1 failed"; exit 1; }
}
build small_fifo "-DMEMSIZE=2048"
build small_lru  "-DMEMSIZE=2048 -DREPLACE_LRU=1"
build global     "-DGLOBAL_REPLACEMENT=1"

# word_at <file> <byte address>: signed 32-bit little-endian word from a .byte/.out file
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <sim> <test dir> <sim arguments...>: fresh directory, batch mode
    local sim="$1" dir="$WORK/$2"; shift 2
    mkdir -p "$dir" && cp "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && (cd "$dir" && "$sim" -b "$@" > run.txt 2>&1)
}

# paging <run.txt> <pid>: "faults zero-fill page-in page-out data-frames-at-end"
paging() {
    awk -v P="$2" '$1 == "[os]" && $2 == "pid" && $3 == P && $4 == "paging" {
        print $7, $10, $13, $15, $17 }' "$1"
}

# totals <run.txt>: total faults minus total page-outs = frames that were free when paging began
free_frames_used() {
    awk '$1 == "[os]" && $4 == "paging" { f += $7; o += $15 } END { print f - o }' "$1"
}

echo "e13: 4 tasks x (1 code + 7 data pages) = 32 frames on 15 frames (local FIFO, 8 KB)"
run "$HERE/sim" four pagewalk.txt pw1_data.byte pagewalk.txt pw2_data.byte \
                     pagewalk.txt pw3_data.byte pagewalk.txt pw4_data.byte
check "loader: each task starts with code + 1 data page, page 1 waits in the backing store" \
      "$(grep -c '1 instruction + 1 data page(s), 1 more in the backing store' "$WORK/four/run.txt")" 4
check "all 4 sums R * (1000 + K*S + 21) correct (at 6*512 + 4)" \
      "$(for s in 1 2 3 4; do word_at "$WORK/four/pw${s}_data_${s}.out" 3076; done | paste -sd, -)" "3081,3099,3117,3135"
O="$WORK/four/pw2_data_2.out"
check "pid 2: page 0 (paged out since round 3) comes back from the backing store: K, R, S = 6,3,2" \
      "$(word_at "$O" 0),$(word_at "$O" 4),$(word_at "$O" 8)" "6,3,2"
check "pid 2: page 1 = 1000 + S + 1, page 3 = S + 3, page 6 = S + 6" \
      "$(word_at "$O" 512),$(word_at "$O" 1536),$(word_at "$O" 3072)" "1003,5,8"
# fill: pages 1..6 fault (page 1 paged in from the data file, 2..6 zero-filled); every read of
# the 3 passes faults (6-page cycle > resident pages); pids 1-3 got 2 free frames, pid 4 only 1
check "pids 1-3: 24 faults = 5 zero-fill + 19 page-in, 22 page-out, 3 data frames" \
      "$(for p in 1 2 3; do paging "$WORK/four/run.txt" $p; done | sort -u)" "24 5 19 22 3"
check "pid 4 (found no free frame in round 2): 24 faults, 23 page-out, 2 data frames" \
      "$(paging "$WORK/four/run.txt" 4)" "24 5 19 23 2"
check "faults - page-outs over all tasks = 7 frames free when the tasks started" \
      "$(free_frames_used "$WORK/four/run.txt")" 7

echo "e13: 2 KB memory (3 frames: code + 2 data frames), local FIFO"
run "$WORK/small_fifo/sim" big pagewalk.txt big_data.byte
check "a task with a 3-page data file starts (base: 'needs more frames than physical memory has')" \
      "$(grep -c '2 more in the backing store, 1 frame(s) left' "$WORK/big/run.txt")" 1
check "big_data: sum 3 * (1000 + 2000 + 6*5 + 21) = 9153, page 2 = 2000 + 5 + 2" \
      "$(word_at "$WORK/big/big_data_1.out" 3076),$(word_at "$WORK/big/big_data_1.out" 1024)" "9153,2007"
check "big_data: 24 faults = 4 zero-fill + 20 page-in (pages 1 and 2 from the file), 23 page-out" \
      "$(paging "$WORK/big/run.txt" 1)" "24 4 20 23 2"
run "$WORK/small_fifo/sim" hot_fifo hotpage.txt pw1_data.byte
check "hotpage FIFO: sum 3081 kept in page 0" "$(word_at "$WORK/hot_fifo/pw1_data_1.out" 12)" 3081
# per pass FIFO faults 9 times: 6 reads + page 0 three times (it is evicted whenever it is oldest)
check "hotpage FIFO: 33 faults = 6 (fill) + 3 passes x 9" "$(paging "$WORK/hot_fifo/run.txt" 1)" "33 5 28 32 2"

echo "e13: 2 KB memory, local LRU (-DREPLACE_LRU=1)"
run "$WORK/small_lru/sim" hot_lru hotpage.txt pw1_data.byte
check "hotpage LRU: same sum 3081" "$(word_at "$WORK/hot_lru/pw1_data_1.out" 12)" 3081
# LRU never evicts page 0 once it is back: 6 (fill) + 7 (pass 1: page 0 once) + 6 + 6
check "hotpage LRU: 25 faults (page 0 stays resident)" "$(paging "$WORK/hot_lru/run.txt" 1)" "25 5 20 24 2"
run "$WORK/small_lru/sim" walk_lru pagewalk.txt pw1_data.byte
run "$WORK/small_fifo/sim" walk_fifo pagewalk.txt pw1_data.byte
check "pagewalk (pure cycle of 6 pages > 2 frames): LRU = FIFO, every read faults" \
      "$(paging "$WORK/walk_lru/run.txt" 1)/$(paging "$WORK/walk_fifo/run.txt" 1)" "24 5 19 23 2/24 5 19 23 2"
check "pagewalk LRU sum 3081" "$(word_at "$WORK/walk_lru/pw1_data_1.out" 3076)" 3081

echo "e13: global replacement (-DGLOBAL_REPLACEMENT=1), the same 4 tasks"
run "$WORK/global/sim" glob pagewalk.txt pw1_data.byte pagewalk.txt pw2_data.byte \
                            pagewalk.txt pw3_data.byte pagewalk.txt pw4_data.byte
check "global: all 4 sums correct" \
      "$(for s in 1 2 3 4; do word_at "$WORK/glob/pw${s}_data_${s}.out" 3076; done | paste -sd, -)" "3081,3099,3117,3135"
check "global: pid 3 page 0 back from the backing store (K, R, S = 6,3,3)" \
      "$(word_at "$WORK/glob/pw3_data_3.out" 0),$(word_at "$WORK/glob/pw3_data_3.out" 4),$(word_at "$WORK/glob/pw3_data_3.out" 8)" "6,3,3"
# pid 4 steals pid 1's page 0 in round 2, so pid 1 is the task left with 2 frames
check "global: pid 1 lost a frame to pid 4 (24 faults, 23 page-out, 2 frames); pids 2-4 have 3" \
      "$(paging "$WORK/glob/run.txt" 1);$(for p in 2 3 4; do paging "$WORK/glob/run.txt" $p; done | sort -u)" \
      "24 5 19 23 2;24 5 19 22 3"
check "global: faults - page-outs = 7" "$(free_frames_used "$WORK/glob/run.txt")" 7

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
