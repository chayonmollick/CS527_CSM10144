#!/usr/bin/env bash
# e20 tests: fork() and getpid().   ./test.sh   (builds ./sim, runs everything in a temp directory)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
make -s -C "$HERE" || { echo "build failed"; exit 1; }
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

run() {     # run <case dir> <sim arguments...>: batch run in a fresh directory, prints the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$HERE"/*.txt "$HERE"/*.byte "$EX"/counter.txt "$EX"/counter_data.byte "$dir"/ \
        && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

echo "Compiler: byte code of fork and getpid"
mkdir -p "$WORK/c" && cp "$HERE/fork_demo.txt" "$WORK/c/"
(cd "$WORK/c" && "$SIM" -c fork_demo.txt > listing.txt 2>&1)
check "x1 = fork   -> 40 01 00 00" "$(grep -c '40 01 00 00   x1 = fork' "$WORK/c/listing.txt")" 1
check "x5 = getpid -> 41 05 00 00" "$(grep -c '41 05 00 00   x5 = getpid' "$WORK/c/listing.txt")" 2
printf 'v1 = fork\n' > "$WORK/c/bad.txt"
check "v1 = fork is a compile error" "$(cd "$WORK/c" && "$SIM" -c bad.txt 2>&1 | grep -c 'usage: x<n> = fork')" 1

echo "fork_demo: parent and child each get their own copy of memory"
check "exit code" "$(run demo fork_demo.txt fork_demo_data.byte)" 0
D="$WORK/demo"
check "os reports the fork" "$(grep -c 'pid 2 created by fork() in pid 1 (2 page(s) copied)' "$D/run.txt")" 1
check "parent [0] = 100" "$(word_at "$D/fork_demo_data_1.out" 0)" 100
check "parent [4] = child pid 2 (fork result)" "$(word_at "$D/fork_demo_data_1.out" 4)" 2
check "parent [8] = getpid 1" "$(word_at "$D/fork_demo_data_1.out" 8)" 1
check "parent [12] = 7 (written before fork)" "$(word_at "$D/fork_demo_data_1.out" 12)" 7
check "child [0] = 200" "$(word_at "$D/fork_demo_data_2.out" 0)" 200
check "child [4] = 0 (parent's later write not seen)" "$(word_at "$D/fork_demo_data_2.out" 4)" 0
check "child [8] = getpid 2" "$(word_at "$D/fork_demo_data_2.out" 8)" 2
check "child [12] = 7 (copied from the parent)" "$(word_at "$D/fork_demo_data_2.out" 12)" 7
check "log: pid 1 prints twice, then the child" "$(awk '{print $3 ":" $6}' "$D/log.txt" | paste -sd, -)" "1:00000001,1:00000001,2:00000002"
check "child's .freq: fork line not executed by the child" "$(grep 'x1 = fork' "$D/fork_demo_2.freq" | cut -f1)" 0
check "parent's .freq: fork line executed once" "$(grep 'x1 = fork' "$D/fork_demo_1.freq" | cut -f1)" 1
check "child's .freq: child branch ran once" "$(grep 'x3 = 200' "$D/fork_demo_2.freq" | cut -f1)" 1

echo "fork next to another task: pids keep counting"
run mix counter.txt counter_data.byte fork_demo.txt fork_demo_data.byte > /dev/null
check "counter (pid 1) still counts to 10" "$(word_at "$WORK/mix/counter_data_1.out" 0)" 10
check "fork_demo is pid 2, its child pid 3" "$(word_at "$WORK/mix/fork_demo_data_2.out" 4),$(word_at "$WORK/mix/fork_demo_data_3.out" 8)" "3,3"

echo "fork_tree: fork; fork; fork = 8 processes on 4 processors"
check "exit code" "$(run tree fork_tree.txt fork_tree_data.byte)" 0
T="$WORK/tree"
check "8 output files" "$(ls "$T" | grep -c '^fork_tree_data_[0-9]*\.out$')" 8
pids=""
for k in 1 2 3 4 5 6 7 8; do pids="$pids$(word_at "$T/fork_tree_data_$k.out" 0) "; done
check "each process stored its own pid at [0]" "$pids" "1 2 3 4 5 6 7 8 "
check "log: 8 prints, pids 1..8" "$(awk '{print $3}' "$T/log.txt" | paste -sd, -)" "1,2,3,4,5,6,7,8"
check "process tree (who forked whom)" "$(grep 'created by fork' "$T/run.txt" | awk '{print $3 "<" $9}' | paste -sd, -)" "2<1,3<1,4<1,5<2,6<2,7<3,8<5"
check "children 6 and 7 waited for a processor" "$(grep 'is waiting' "$T/run.txt" | awk '{print $3}' | paste -sd, -)" "6,7"

echo "fork_bigmem: the child waits for frames and copies demand-mapped pages"
check "exit code" "$(run big fork_bigmem.txt fork_bigmem_data.byte)" 0
B="$WORK/big"
check "8 pages copied (1 instruction + 7 data)" "$(grep -c 'pid 2 created by fork() in pid 1 (8 page(s) copied)' "$B/run.txt")" 1
check "child waited (8 frames needed, 7 free)" "$(grep -c 'pid 2 is waiting' "$B/run.txt")" 1
check "child loaded after the parent finished" "$(grep -c 'pid 2 (fork_bigmem) loaded on processor 0: 1 instruction + 7 data page(s)' "$B/run.txt")" 1
check "parent: pid 1, [512] = 11, fork gave 2" "$(word_at "$B/fork_bigmem_data_1.out" 0),$(word_at "$B/fork_bigmem_data_1.out" 4),$(word_at "$B/fork_bigmem_data_1.out" 8)" "1,11,2"
check "child: pid 2, [512] = 11, fork gave 0" "$(word_at "$B/fork_bigmem_data_2.out" 0),$(word_at "$B/fork_bigmem_data_2.out" 4),$(word_at "$B/fork_bigmem_data_2.out" 8)" "2,11,0"

echo "fork_bomb: the process table is full after MAX_TASKS = 64 tasks"
check "exit code" "$(run bomb fork_bomb.txt fork_tree_data.byte)" 0
check "63 children created" "$(grep -c 'created by fork' "$WORK/bomb/run.txt")" 63
check "later forks fail (fork returns -1)" "$(grep -c 'process table full' "$WORK/bomb/run.txt" | awk '{print ($1 > 0)}')" 1
check "64 tasks finished, one output each" "$(ls "$WORK/bomb" | grep -c '^fork_tree_data_[0-9]*\.out$')" 64

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
