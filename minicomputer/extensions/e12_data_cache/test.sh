#!/usr/bin/env bash
# e12 tests: per-processor data cache (direct-mapped, 16 lines x 16 bytes, physical addresses).
#   ./test.sh          builds ./sim (write-back, make -s) and a write-through copy in a scratch
#                      directory (-DWRITE_BACK=0), runs the programs, checks results and statistics
# The expected numbers are derived by hand in README.md.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/wt-build" && cp "$HERE"/*.cpp "$HERE"/*.h "$HERE"/Makefile "$WORK/wt-build"/ &&
    (cd "$WORK/wt-build" && make -s CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DWRITE_BACK=0") ||
    { echo "write-through build failed"; exit 1; }
WB="$HERE/sim"
WT="$WORK/wt-build/sim"
pass=0; fail=0

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
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && rm -f "$dir/log.txt" \
        && (cd "$dir" && "$sim" -b "$@" > run.txt 2>&1)
}

# dc <run.txt> <pid>: "accesses hits misses write-backs write-backs-at-end bytes-to-memory"
dc() {
    awk -v P="$2" '$1 == "[os]" && $2 == "pid" && $3 == P && $4 == "D-cache" {
        line = $0; gsub(/[(),:%]/, " ", line); n = split(line, w, " ")
        for (i = 2; i <= n; i++) {
            if (w[i] == "accesses") a = w[i-1]
            if (w[i] == "hits") h = w[i-1]
            if (w[i] == "misses") m = w[i-1]
            if (w[i] == "write-back") wb = w[i-1]
            if (w[i] == "at" && w[i+1] == "the") e = w[i-1]
            if (w[i] == "bytes") by = w[i-1]
        }
        print a, h, m, wb, e, by }' "$1"
}

echo "e12 write-back: sum_array (sequential words, 4 per line)"
run "$WB" sa sum_array.txt sum_array_data.byte
check "sum_array result 45 (the dirty line was flushed before the output was written)" \
      "$(word_at "$WORK/sa/sum_array_data_1.out" 36)" 45
# 11 word accesses at 0,4,...,36 (+ store 36): lines 0,1,2 miss once each -> 8 hits; 1 dirty line at the end
check "sum_array: 11 accesses, 8 hits, 3 misses, 1 write-back (at the end), 16 bytes" \
      "$(dc "$WORK/sa/run.txt" 1)" "11 8 3 1 1 16"

echo "e12 write-back: stride.txt (same program, stride 4 vs 16 bytes)"
run "$WB" s4 stride.txt stride4_data.byte
check "stride 4, N = 32: sum 1..32 = 528" "$(word_at "$WORK/s4/stride4_data_1.out" 8)" 528
# [0] miss, [4] hit, 32 loads in 8 lines (8 misses), [8] store hit
check "stride 4: 35 accesses, 26 hits, 9 misses" "$(dc "$WORK/s4/run.txt" 1 | cut -d' ' -f1-3)" "35 26 9"
run "$WB" s16 stride.txt stride16_data.byte
check "stride 16, N = 15: sum 1+5+..+57 = 435" "$(word_at "$WORK/s16/stride16_data_1.out" 8)" 435
# every one of the 15 loads is in a new line
check "stride 16: 18 accesses, 2 hits, 16 misses" "$(dc "$WORK/s16/run.txt" 1 | cut -d' ' -f1-3)" "18 2 16"

echo "e12 write-back: dot.txt (conflict misses of a direct-mapped cache)"
run "$WB" d256 dot.txt dot256_data.byte
check "b = a + 256: dot product 1^2+..+16^2 = 1496" "$(word_at "$WORK/d256/dot256_data_1.out" 8)" 1496
# a[i] and b[i] are 256 bytes apart = same line index, different tags: all 32 loads miss
check "b = a + 256: 35 accesses, 2 hits, 33 misses" "$(dc "$WORK/d256/run.txt" 1 | cut -d' ' -f1-3)" "35 2 33"
run "$WB" d272 dot.txt dot272_data.byte
check "b = a + 272: same result 1496" "$(word_at "$WORK/d272/dot272_data_1.out" 8)" 1496
check "b = a + 272 (one line of padding): 35 accesses, 26 hits, 9 misses" \
      "$(dc "$WORK/d272/run.txt" 1 | cut -d' ' -f1-3)" "35 26 9"

echo "e12 write-back: memsum.txt (running sum kept in memory at [8], N = 64)"
run "$WB" ms memsum.txt memsum_data.byte
check "memsum result 1+..+64 = 2080" "$(word_at "$WORK/ms/memsum_data_1.out" 8)" 2080
# 1 + 3 x 64 accesses; misses: [0], 15 array lines, then a[60..63] and [8] evict each other 4 times;
# the dirty [8] line is written back 4 times by those evictions + once at the end = 5 x 16 bytes
check "memsum: 193 accesses, 169 hits, 24 misses, 5 write-backs (1 at the end), 80 bytes" \
      "$(dc "$WORK/ms/run.txt" 1)" "193 169 24 5 1 80"

echo "e12 write-back: array_add (vector loads/stores, base example)"
run "$WB" aa array_add.txt array_add_data.byte
check "array_add C[0] = 101, C[15] = 131" \
      "$(word_at "$WORK/aa/array_add_data_1.out" 132),$(word_at "$WORK/aa/array_add_data_1.out" 192)" "101,131"
# 49 words in 13 distinct lines (0..12), no conflicts; C occupies 5 dirty lines
check "array_add: 49 accesses, 36 hits, 13 misses, 5 write-backs, 80 bytes" \
      "$(dc "$WORK/aa/run.txt" 1)" "49 36 13 5 5 80"

echo "e12 write-back: 4 tasks at once (each processor has its own cache)"
run "$WB" multi sum_array.txt sum_array_data.byte stride.txt stride4_data.byte \
                dot.txt dot256_data.byte memsum.txt memsum_data.byte
check "4 tasks: same statistics as alone" \
      "$(for p in 1 2 3 4; do dc "$WORK/multi/run.txt" $p | cut -d' ' -f3; done | paste -sd, -)" "3,9,33,24"
check "4 tasks: all results correct" "$(word_at "$WORK/multi/sum_array_data_1.out" 36),$(word_at \
      "$WORK/multi/stride4_data_2.out" 8),$(word_at "$WORK/multi/dot256_data_3.out" 8),$(word_at \
      "$WORK/multi/memsum_data_4.out" 8)" "45,528,1496,2080"

echo "e12 write-through + no-write-allocate (built with -DWRITE_BACK=0)"
run "$WT" tsa sum_array.txt sum_array_data.byte
check "WT sum_array result 45" "$(word_at "$WORK/tsa/sum_array_data_1.out" 36)" 45
check "WT sum_array: same 8 hits, 0 write-backs, 4 bytes (one store)" "$(dc "$WORK/tsa/run.txt" 1)" "11 8 3 0 0 4"
run "$WT" tms memsum.txt memsum_data.byte
check "WT memsum result 2080" "$(word_at "$WORK/tms/memsum_data_1.out" 8)" 2080
check "WT memsum: same hits, 0 write-backs, 64 stores x 4 = 256 bytes" "$(dc "$WORK/tms/run.txt" 1)" "193 169 24 0 0 256"
run "$WT" taa array_add.txt array_add_data.byte
check "WT array_add C[0] = 101, C[15] = 131" \
      "$(word_at "$WORK/taa/array_add_data_1.out" 132),$(word_at "$WORK/taa/array_add_data_1.out" 192)" "101,131"
# no-write-allocate: the 16 stores to C never bring C's lines in, so all 16 miss; B[15] (line 8) misses too
check "WT array_add: 49 accesses, 24 hits, 25 misses, 16 stores x 4 = 64 bytes" "$(dc "$WORK/taa/run.txt" 1)" "49 24 25 0 0 64"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
