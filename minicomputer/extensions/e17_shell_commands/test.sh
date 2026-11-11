#!/usr/bin/env bash
# e17 tests: shell commands ps, kill, mem, help (and sleep, which makes piped scripts deterministic).
#   ./test.sh      (builds ./sim, runs everything in a temp directory)
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

# ps_rows <pid>: "STATE/PROC/INSTR/PAGES" of that pid in every ps listing of run.txt, comma separated
ps_rows() {
    awk -v P="$1" '$1 == P && $3 ~ /^(WAITING|READY|DONE|FAILED|KILLED)$/ {
                       printf "%s%s/%s/%s/%s", sep, $3, $4, $5, $6; sep = "," }
                   END { print "" }' run.txt
}

echo "Scenario 1: 3 endless loops + 2 counters on 4 processors; ps, mem, kill"
mkdir -p "$WORK/s1" && cp "$HERE/loop.txt" "$HERE/loop_data.byte" "$EX/counter.txt" "$EX/counter_data.byte" "$WORK/s1"/
cd "$WORK/s1"
# one small file piped in one write: every line before 'sleep 3' is read before round 1
printf '%s\n' 'loop.txt loop_data.byte' 'loop.txt loop_data.byte' 'loop.txt loop_data.byte' \
    'counter.txt counter_data.byte' 'counter.txt counter_data.byte' 'sleep 3' 'ps' 'mem' 'kill 5' 'kill 2' \
    'ps' 'mem' 'counter.txt counter_data.byte' 'mem' 'kill 2' 'kill 9' 'frobnicate' 'sleep 3' 'ps' \
    'kill 1' 'kill 3' 'help' 'exit' > cmds.txt
cat cmds.txt | "$SIM" > run.txt 2>&1
check "simulator exits normally (exit code 0)" "$?" 0
check "pid 5 had to wait (4 processors busy)" "$(grep -c 'pid 5 is waiting' run.txt)" 1
check "ps: pid 1 after 3, 3 and 6 rounds" "$(ps_rows 1)" "READY/0/30/2,READY/0/30/2,READY/0/60/2"
check "ps: pid 2 running, then killed" "$(ps_rows 2)" "READY/1/30/2,KILLED/-/30/0,KILLED/-/30/0"
check "ps: pid 4 (counter) runs, then done" "$(ps_rows 4)" "READY/3/30/2,READY/3/30/2,DONE/-/43/0"
check "ps: pid 5 waiting, then killed" "$(ps_rows 5)" "WAITING/-/0/0,KILLED/-/0/0,KILLED/-/0/0"
check "ps: pid 6 reuses processor 1" "$(ps_rows 6)" "READY/1/30/2"
check "mem: free frames 7, 9 (after kills), 7 (pid 6)" \
      "$(grep -o '[0-9]* of 15 frames free' run.txt | awk '{print $1}' | paste -sd, -)" "7,9,7"
check "mem: frames 3,4 freed by kill 2, then given to pid 6" \
      "$(grep '^\[mem\] owner' run.txt | awk '{print $6 $7}' | paste -sd, -)" "22,..,66"
check "kill of a waiting task" "$(grep -c 'pid 5 (counter) KILLED while waiting' run.txt)" 1
check "kill of a running task frees processor 1" "$(grep -c 'pid 2 (loop) KILLED on processor 1 after 30 instructions' run.txt)" 1
check "kill of an already killed task is refused" "$(grep -c 'pid 2 is not running or waiting (it is KILLED)' run.txt)" 1
check "kill of an unknown pid is refused" "$(grep -c 'no task with pid 9' run.txt)" 1
check "unknown command and help both print the help" "$(grep -c '\[shell\] usage:' run.txt),$(grep -c 'kill <pid>' run.txt)" "2,2"
check "killed pid 2 wrote its data: [0] = 10 after 30 instructions" "$(word_at loop_data_2.out 0)" 10
check "killed pid 1 wrote its data: [0] = 20 after 60 instructions" "$(word_at loop_data_1.out 0)" 20
check "killed pid 3 wrote its data: [0] = 20" "$(word_at loop_data_3.out 0)" 20
check "killed pid 2 wrote its .freq (counts at the kill)" "$(cut -f1 loop_2.freq | paste -sd, -)" "1,.loop,10,10,9"
check "waiting pid 5 killed: output = initial data" "$(word_at counter_data_5.out 0)" 0
check "counters 4 and 6 still finish correctly" "$(word_at counter_data_4.out 0),$(word_at counter_data_6.out 0)" "10,10"
check "pid 5 never printed, pids 4 and 6 printed 10 lines each" \
      "$(grep -c 'Process id: 5 ' log.txt),$(grep -c 'Process id: 4 ' log.txt),$(grep -c 'Process id: 6 ' log.txt)" "0,10,10"

echo "Scenario 2: end of input while sleeping; last line has no newline; a failed task in ps"
mkdir -p "$WORK/s2" && cp "$HERE/loop.txt" "$HERE/loop_data.byte" "$WORK/s2"/
cd "$WORK/s2"
printf 'x1 = 5\nx2 = 0\nx3 = x1 / x2\n' > div0.txt
printf 'loop.txt loop_data.byte\ndiv0.txt loop_data.byte\nsleep 2\nps\nkill 1' > cmds.txt
"$SIM" < cmds.txt > run.txt 2>&1       # a file, so end of input is seen at once (a pipe could be later)
check "exit code 1 (one task failed)" "$?" 1
check "ps after 2 rounds: loop running, div0 failed" "$(ps_rows 1);$(ps_rows 2)" "READY/0/20/2;FAILED/-/3/0"
check "last line 'kill 1' ran after the sleep: [0] = 6 after 20 instructions" "$(word_at loop_data_1.out 0)" 6
check "shell closed at end of input" "$(grep -c 'end of input: no new tasks' run.txt)" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
