#!/usr/bin/env bash
# e21 tests: exit codes, fork + wait, zombies, orphans, deadlock recovery.
#   ./test.sh   (builds ./sim, runs everything in a temp directory)
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

table() {   # table <run.txt>: "pid:state:code:reaper" for every row of the final exit-code table
    awk '/^\[os\] exit codes:/ { f = 1; next }
         f && $1 ~ /^[0-9]+$/ { by = ($7 == "pid") ? $8 : $7
                                printf "%s%s:%s:%s:%s", sep, $1, $4, $5, by; sep = "," }
         END { print "" }' "$1"
}

echo "Compiler: byte code of exit, wait, fork"
mkdir -p "$WORK/c" && cp "$HERE/wait_demo.txt" "$WORK/c/"
(cd "$WORK/c" && "$SIM" -c wait_demo.txt > listing.txt 2>&1)
check "x1 = fork     -> 40 01 00 00" "$(grep -c '40 01 00 00   x1 = fork' "$WORK/c/listing.txt")" 1
check "x4 = wait x5  -> 43 04 01 05" "$(grep -c '43 04 01 05   x4 = wait x5' "$WORK/c/listing.txt")" 1
check "x6 = wait     -> 43 06 00 00" "$(grep -c '43 06 00 00   x6 = wait ' "$WORK/c/listing.txt")" 1
check "exit x10      -> 44 00 00 0A" "$(grep -c '44 00 00 0A   exit x10' "$WORK/c/listing.txt")" 1
printf 'exit 5\n' > "$WORK/c/bad1.txt"; printf 'x1 = wait 3\n' > "$WORK/c/bad2.txt"
check "exit 5 is a compile error" "$(cd "$WORK/c" && "$SIM" -c bad1.txt 2>&1 | grep -c 'usage: exit x<n>')" 1
check "x1 = wait 3 is a compile error" "$(cd "$WORK/c" && "$SIM" -c bad2.txt 2>&1 | grep -c 'usage: x<n> = wait')" 1

echo "exit xR: exit codes without fork"
check "exit code of the run (a task failed)" "$(run ex exit_codes.txt exit_data.byte counter.txt counter_data.byte exit_fail.txt exit_data.byte)" 1
check "exit x1 ended the task: [0] = 7, not 99" "$(word_at "$WORK/ex/exit_data_1.out" 0)" 7
check "exit_codes ran 3 instructions" "$(grep -c 'pid 1 (exit_codes) finished on processor 0 after 3 instructions, 0 page fault(s), exit code 7' "$WORK/ex/run.txt")" 1
check "table: exit 7, normal end 0, error -1" "$(table "$WORK/ex/run.txt")" "1:exited:7:os,2:exited:0:os,3:error:-1:os"

echo "wait_demo: wait() collects a zombie at once, blocks for running children"
check "exit code" "$(run wd wait_demo.txt wait_demo_data.byte)" 0
W="$WORK/wd"
check "parent [0] = 10 + 20 + 30" "$(word_at "$W/wait_demo_data_1.out" 0)" 60
check "parent [4] = -1 (wait with no children)" "$(word_at "$W/wait_demo_data_1.out" 4)" -1
check "children [0] = 10, 20, 30" "$(word_at "$W/wait_demo_data_2.out" 0),$(word_at "$W/wait_demo_data_3.out" 0),$(word_at "$W/wait_demo_data_4.out" 0)" "10,20,30"
check "child 1 (pid 2) was a zombie" "$(grep -c 'pid 2 is a zombie until pid 1 calls wait()' "$W/run.txt")" 1
check "wait() results in order: pid:code" "$(grep 'wait() returns pid' "$W/run.txt" | awk '{sub(",", "", $7); print $7 ":" $10}' | paste -sd, -)" "2:10,4:30,3:20"
check "the zombie was collected without blocking" "$(grep -E 'returns pid 2|blocks in wait' "$W/run.txt" | head -1 | grep -c 'returns pid 2')" 1
check "the parent blocked twice" "$(grep -c 'pid 1 blocks in wait()' "$W/run.txt")" 2
check "the 4th wait returned -1" "$(grep -c 'pid 1: wait() returns -1 (no children left)' "$W/run.txt")" 1
check "log: the parent printed pids 2, 4, 3" "$(awk '{print $3 ":" $6}' "$W/log.txt" | paste -sd, -)" "1:00000002,1:00000004,1:00000003"
check "table: codes and who reaped them" "$(table "$W/run.txt")" "1:exited:60:os,2:exited:10:1,3:exited:20:1,4:exited:30:1"

echo "zombie_orphan: a parent that never waits"
check "exit code" "$(run zo zombie_orphan.txt exit_data.byte)" 0
Z="$WORK/zo"
check "child A (pid 2) is a zombie while the parent runs" "$(grep -c 'pid 2 is a zombie until pid 1 calls wait()' "$Z/run.txt")" 1
check "the zombie is reaped by the os when the parent ends" "$(grep -c 'pid 2 (zombie child of pid 1) reaped by the os' "$Z/run.txt")" 1
check "child B (pid 3) is an orphan" "$(grep -c 'pid 3 is an orphan (pid 1 has ended): reaped by the os' "$Z/run.txt")" 1
check "table: parent 5, child A 1, child B 2, all reaped by the os" "$(table "$Z/run.txt")" "1:exited:5:os,2:exited:1:os,3:exited:2:os"

echo "deadlock: 4 blocked parents hold all processors, pid 5 waits"
check "exit code (the victim is reported as an error)" "$(run dl deadlock.txt exit_data.byte)" 1
L="$WORK/dl"
check "pid 5 had to wait" "$(grep -c 'pid 5 is waiting' "$L/run.txt")" 1
check "deadlock detected, newest blocked task stopped" "$(grep -c 'deadlock: .* stopping pid 4$' "$L/run.txt")" 1
check "pid 3's wait() got the victim's code -1" "$(grep -c 'pid 3: wait() returns pid 4, exit code -1' "$L/run.txt")" 1
check "pid 5 then ran as an orphan" "$(grep -c 'pid 5 is an orphan (pid 4 has ended)' "$L/run.txt")" 1
check "each finished process stored its pid" "$(word_at "$L/exit_data_1.out" 0),$(word_at "$L/exit_data_2.out" 0),$(word_at "$L/exit_data_3.out" 0),$(word_at "$L/exit_data_5.out" 0)" "1,2,3,5"
check "table" "$(table "$L/run.txt")" "1:exited:1:os,2:exited:2:1,3:exited:3:2,4:error:-1:3,5:exited:4:os"

echo "From the shell"
mkdir -p "$WORK/sh" && cp "$HERE"/wait_demo* "$WORK/sh"/
(cd "$WORK/sh" && printf 'wait_demo.txt wait_demo_data.byte\nexit\n' | "$SIM" > run.txt 2>&1)
check "wait_demo started by the shell: [0] = 60" "$(word_at "$WORK/sh/wait_demo_data_1.out" 0)" 60
check "exit-code table printed" "$(table "$WORK/sh/run.txt")" "1:exited:60:os,2:exited:10:1,3:exited:20:1,4:exited:30:1"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
