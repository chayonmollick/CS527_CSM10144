#!/usr/bin/env bash
# e11 tests: per-processor TLB (4 entries, fully associative, LRU).
#   ./test.sh          builds ./sim (make -s), runs the programs in a scratch directory
# The expected numbers are derived by hand in README.md ("Solution idea", "Expected output").
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
SIM="$HERE/sim"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
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

run() {     # run <test dir> <sim arguments...>: fresh directory, batch mode, prints the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && rm -f "$dir/log.txt" \
        && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

# tlb <run.txt> <pid>: "lookups hits misses fetch-lookups data-lookups data-hits" from the TLB line
tlb() {
    awk -v P="$2" '$1 == "[os]" && $2 == "pid" && $3 == P && $4 == "TLB:" {
        line = $0; gsub(/[(),;=%]/, " ", line); n = split(line, w, " ")
        for (i = 1; i <= n; i++) {
            if (w[i] == "lookups") lookups = w[i-1]
            if (w[i] == "hits") hits = w[i-1]
            if (w[i] == "misses") misses = w[i-1]
            if (w[i] == "fetch:") fetch = w[i+3]
            if (w[i] == "data:") { dhit = w[i+1]; data = w[i+3] }
        }
        print lookups, hits, misses, fetch, data, dhit }' "$1"
}

echo "e11: sum_array (one code page, one data page)"
check "sum_array exit code" "$(run sa sum_array.txt sum_array_data.byte)" 0
check "sum_array result 45" "$(word_at "$WORK/sa/sum_array_data_1.out" 36)" 45
# 71 fetches + 11 data words = 82 lookups; only the first fetch and the first data access miss
check "sum_array TLB: 82 lookups, 80 hits, 2 misses, 71 fetch, 11 data (10 hit)" \
      "$(tlb "$WORK/sa/run.txt" 1)" "82 80 2 71 11 10"

echo "e11: tlb_walk, K = 3 data pages, R = 4 passes (fits: code page + 3 data pages = 4 entries)"
run k3 tlb_walk.txt tlb3_data.byte > /dev/null
check "K=3 sum = 4 * (1+2+3) = 24" "$(word_at "$WORK/k3/tlb3_data_1.out" 8)" 24
# 151 instructions; data lookups 2 + 3 + 12 + 1 = 18; misses: code page + 3 data pages = 4
check "K=3 TLB: 169 lookups, 165 hits, 4 misses, 151 fetch, 18 data (15 hit)" \
      "$(tlb "$WORK/k3/run.txt" 1)" "169 165 4 151 18 15"

echo "e11: tlb_walk, K = 5 data pages, R = 4 passes (5 data pages > 3 free entries: LRU thrashes)"
run k5 tlb_walk.txt tlb5_data.byte > /dev/null
check "K=5 sum = 4 * (1+..+5) = 60" "$(word_at "$WORK/k5/tlb5_data_1.out" 8)" 60
check "K=5 page p holds p + 1 (page 4, word 3)" "$(word_at "$WORK/k5/tlb5_data_1.out" $((4 * 512 + 12)))" 5
# misses = code 1 + [0] 1 + fill pages 1..4 + every read 5 * 4 + final [8] 1 = 27; data hits: [4] and the fill store to page 0
check "K=5 TLB: 249 lookups, 222 hits, 27 misses, 221 fetch, 28 data (2 hit)" \
      "$(tlb "$WORK/k5/run.txt" 1)" "249 222 27 221 28 2"

echo "e11: 5 tasks on 4 processors (pid 5 reuses processor 0: its TLB must be flushed)"
# K = 2 for pids 1-4 (3 frames each) so that all pages fit in the 15 frames (no replacement in this simulator)
run multi tlb_walk.txt tlb2_data.byte tlb_walk.txt tlb2_data.byte tlb_walk.txt tlb2_data.byte \
          tlb_walk.txt tlb2_data.byte tlb_walk.txt tlb5_data.byte > /dev/null
check "pid 5 waited, then ran on processor 0" "$(grep -c 'pid 5 (tlb_walk) loaded on processor 0' "$WORK/multi/run.txt")" 1
# K = 2: 116 instructions, data lookups 2 + 2 + 8 + 1 = 13, misses: code page + 2 data pages
check "pids 1-4 (K=2) each: 129 lookups, 3 misses (per-processor TLBs do not disturb each other)" \
      "$(for p in 1 2 3 4; do tlb "$WORK/multi/run.txt" $p | awk '{print $1 "/" $3}'; done | paste -sd, -)" "129/3,129/3,129/3,129/3"
check "pid 5: same 27 misses as when it runs alone (TLB flushed on load)" "$(tlb "$WORK/multi/run.txt" 5)" "249 222 27 221 28 2"
check "pid 1 sum 4 * (1+2) = 12, pid 5 sum 60" \
      "$(word_at "$WORK/multi/tlb2_data_1.out" 8),$(word_at "$WORK/multi/tlb5_data_5.out" 8)" "12,60"

echo "e11: an unaligned word that crosses a page boundary (translated byte by byte)"
mkdir -p "$WORK/cross" && cd "$WORK/cross" || exit 1
printf 'x1 = 255\nx1 = x1 + 255\nx2 = 200\nx2 = x2 * 200\n[x1] = x2\nx3 = [x1]\n[0] = x3\n' > cross.txt
printf '00 00 00 00\n' > d.byte
"$SIM" -b cross.txt d.byte > run.txt 2>&1
check "word at 510..513 written and read back: 40000" "$(word_at d_1.out 0),$(word_at d_1.out 510)" "40000,40000"
# store: 4 byte lookups (page 0 miss, hit, page 1 miss, hit); load: 4 hits; [0]: 1 hit; 8 fetches, 1 miss
check "cross TLB: 17 lookups, 14 hits, 3 misses, 8 fetch, 9 data (7 hit)" "$(tlb run.txt 1)" "17 14 3 8 9 7"
check "cross: page 1 demand-mapped (1 page fault)" "$(grep -c '1 page fault' run.txt)" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
