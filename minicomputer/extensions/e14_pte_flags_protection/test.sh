#!/usr/bin/env bash
# e14 tests: page-table entry flags (VALID, DIRTY, REFERENCED) and STRICT_MEMORY protection.
#   ./test.sh          builds ./sim (make -s, STRICT_MEMORY 0) and a strict copy in a scratch
#                      directory (-DSTRICT_MEMORY=1), runs the programs, checks results and reports
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/strict-build" && cp "$HERE"/*.cpp "$HERE"/*.h "$HERE"/Makefile "$WORK/strict-build"/ &&
    (cd "$WORK/strict-build" && make -s CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DSTRICT_MEMORY=1") ||
    { echo "strict build failed"; exit 1; }
SIM="$HERE/sim"
STRICT="$WORK/strict-build/sim"
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

run() {     # run <sim> <test dir> <sim arguments...>: fresh directory, batch mode, prints the exit code
    local sim="$1" dir="$WORK/$2"; shift 2
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && rm -f "$dir/log.txt" \
        && (cd "$dir" && "$sim" -b "$@" > run.txt 2>&1)
    echo $?
}

# pages <run.txt> <pid>: "mapped referenced dirty copied skipped" from the data-pages report
pages() {
    awk -v P="$2" '$1 == "[os]" && $2 == "pid" && $3 == P && $4 == "data" && $5 == "pages:" {
        print $6, $8, $10, $15, $19 }' "$1"
}

echo "e14 (STRICT_MEMORY 0): flags in the page-table entry, only DIRTY pages copied back"
run "$SIM" sa sum_array.txt sum_array_data.byte > /dev/null
check "sum_array result 45" "$(word_at "$WORK/sa/sum_array_data_1.out" 36)" 45
check "sum_array: 1 data page, referenced and dirty (the final store)" "$(pages "$WORK/sa/run.txt" 1)" "1 1 1 1 0"
run "$SIM" pg pages.txt pages_data.byte > /dev/null
check "pages: 7 + 7 + 7 = 21, 7 at 3825 (demand-mapped pages are still allowed)" \
      "$(word_at "$WORK/pg/pages_data_1.out" 0),$(word_at "$WORK/pg/pages_data_1.out" 3825)" "21,7"
check "pages: data pages 0, 1, 3, 7 mapped, all referenced and dirty" "$(pages "$WORK/pg/run.txt" 1)" "4 4 4 4 0"
run "$SIM" pa partial.txt partial_data.byte > /dev/null
O="$WORK/pa/partial_data_1.out"
check "partial: dot product 1*10 + 2*20 + ... + 8*80 = 2040" "$(word_at "$O" 4)" 2040
check "partial: 3 pages mapped and referenced, only page 0 dirty -> 1 copied, 2 skipped" \
      "$(pages "$WORK/pa/run.txt" 1)" "3 3 1 1 2"
check "partial: the skipped clean pages are still correct (A[7] = 8, B[7] = 80)" \
      "$(word_at "$O" 540),$(word_at "$O" 1052)" "8,80"
printf 'x1 = [0]\nprint x1\n' > "$WORK/ro.txt"
run "$SIM" ro "$WORK/ro.txt" partial_data.byte > /dev/null
check "read-only program: 3 mapped, 1 referenced, 0 dirty -> nothing copied back" "$(pages "$WORK/ro/run.txt" 1)" "3 1 0 0 3"
check "read-only program: the .out file is identical to the data file" \
      "$(cmp -s "$HERE/partial_data.byte" "$WORK/ro/partial_data_1.out" && echo same)" same
run "$SIM" four partial.txt partial_data.byte pages.txt pages_data.byte \
                sum_array.txt sum_array_data.byte "$WORK/ro.txt" partial_data.byte > /dev/null
check "4 tasks at once: each processor's page table has its own flags" \
      "$(for p in 1 2 3 4; do pages "$WORK/four/run.txt" $p; done | paste -sd, -)" "3 3 1 1 2,4 4 4 4 0,1 1 1 1 0,3 1 0 0 3"

echo "e14 (STRICT_MEMORY 1): a data page that is not in the data file is a segmentation fault"
check "pages.txt stops (exit code 1)" "$(run "$STRICT" spg pages.txt pages_data.byte)" 1
check "store to address 1020 (data page 1) -> segmentation fault, at instruction 3" \
      "$(grep -c 'error at instruction 3: segmentation fault: data address 1020 is in data page 1' "$WORK/spg/run.txt")" 1
check "the task is reported as stopped" "$(grep -c 'pid 1 (pages) STOPPED WITH AN ERROR' "$WORK/spg/run.txt")" 1
printf 'x1 = 255\nx1 = x1 * 8\nx2 = [x1]\n' > "$WORK/readfar.txt"
run "$STRICT" sfar "$WORK/readfar.txt" partial_data.byte > /dev/null
check "a load from data page 3 (not in the 3-page file) faults too (bytes are read high first: 2043)" \
      "$(grep -c 'segmentation fault: data address 2043 is in data page 3' "$WORK/sfar/run.txt")" 1
run "$STRICT" spa partial.txt partial_data.byte > /dev/null
check "partial.txt only uses pages of its data file: still runs, 2040" "$(word_at "$WORK/spa/partial_data_1.out" 4)" 2040
printf 'x1 = 7\n[100] = x1\n' > "$WORK/tail.txt"
run "$STRICT" stail "$WORK/tail.txt" sum_array_data.byte > /dev/null
check "protection is per page: [100] is past the 40-byte file but in page 0 -> allowed" \
      "$(word_at "$WORK/stail/sum_array_data_1.out" 100)" 7

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
