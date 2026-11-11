#!/usr/bin/env bash
# e19 tests: Gantt chart, task times and utilisation in timeline.txt.
#   ./test.sh   (builds ./sim, runs everything in a temp directory)
# work.txt runs 3N + 4 instructions: N = 2, 5, 8, 15 (work1/2/3/5.byte) -> 1, 2, 3, 5 rounds.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
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

# readers for timeline.txt
chart()  { awk '/^round/ {on = 1} on && /^$/ {exit} on' "$1"; }          # first block of the chart
times()  { awk '/^pid program/ {on = 1; next} /^average/ {on = 0}
                on {printf "%s%s:%s:%s:%s:%s:%s:%s", sep, $1, $3, $4, $5, $6, $7, $8; sep = " "} END {print ""}' "$1"; }
avgs()   { awk '/^average waiting/ {print $3, $7}' "$1"; }
util()   { awk '/^processor utilisation/ {on = 1; next} on {printf "%s%s=%s", sep, $1, $5; sep = " "} END {print ""}' "$1"; }

newdir() { mkdir -p "$WORK/$1" && cp "$HERE"/work.txt "$HERE"/work*.byte "$WORK/$1"/; }

echo "Five tasks of 3, 1, 2, 5, 2 rounds on NP = 4 (pid 5 waits for pid 2's processor)"
newdir five
(cd "$WORK/five" && "$SIM" -b work.txt work3.byte work.txt work1.byte work.txt work2.byte \
                              work.txt work5.byte work.txt work2.byte > run.txt 2>&1); rc=$?
T="$WORK/five/timeline.txt"
check "exit code" "$rc" 0
check "timeline.txt announced" "$(grep -c 'written to timeline.txt' "$WORK/five/run.txt")" 1
expected_chart='round 1 2 3 4 5
P0    1 1 1 . .
P1    2 5 5 . .
P2    3 3 . . .
P3    4 4 4 4 4'
check "Gantt chart = hand-derived chart" "$(chart "$T")" "$expected_chart"
check "pid:arrival:start:finish:run:waiting:turnaround" "$(times "$T")" \
      "1:0:1:3:3:0:3 2:0:1:1:1:0:1 3:0:1:2:2:0:2 4:0:1:5:5:0:5 5:0:2:3:2:1:3"
check "average waiting 0.20, turnaround 2.80" "$(avgs "$T")" "0.20 2.80"
check "utilisation 60 60 40 100, all 65 %" "$(util "$T")" "P0=60.0 P1=60.0 P2=40.0 P3=100.0 all=65.0"
check "results N(N+1)/2 = 36 3 15 120 15" \
      "$(for f in work3_1 work1_2 work2_3 work5_4 work2_5; do word_at "$WORK/five/$f.out" 4; done | paste -sd' ' -)" "36 3 15 120 15"

echo "One long task: 46 rounds, the chart wraps after GANTT_COLUMNS = 40 rounds"
newdir long
printf '96 00 00 00\n' > "$WORK/long/work150.byte"                     # N = 150: 454 instructions
(cd "$WORK/long" && "$SIM" -b work.txt work150.byte > run.txt 2>&1)
T="$WORK/long/timeline.txt"
check "two chart blocks" "$(grep -c '^round' "$T")" 2
check "second block = rounds 41..46" "$(grep '^round' "$T" | sed -n 2p)" "round 41 42 43 44 45 46"
check "second block, P0 row" "$(grep '^P0 ' "$T" | sed -n 2p)" "P0     1  1  1  1  1  1"
check "idle processors shown as '.'" "$(grep '^P3 ' "$T" | sed -n 2p)" "P3     .  .  .  .  .  ."
check "utilisation 100 0 0 0, all 25 %" "$(util "$T")" "P0=100.0 P1=0.0 P2=0.0 P3=0.0 all=25.0"
check "times: 46 rounds, no waiting" "$(times "$T")" "1:0:1:46:46:0:46"
check "result 150*151/2" "$(word_at "$WORK/long/work150_1.out" 4)" 11325

echo "Shell: tasks typed while the OS is idle all arrive in round 0"
newdir sh
(cd "$WORK/sh" && printf 'work.txt work3.byte\nwork.txt work1.byte\nexit\n' | "$SIM" > run.txt 2>&1)
T="$WORK/sh/timeline.txt"
expected_chart='round 1 2 3
P0    1 1 1
P1    2 . .
P2    . . .
P3    . . .'
check "shell: Gantt chart" "$(chart "$T")" "$expected_chart"
check "shell: utilisation (4 busy of 12 cells)" "$(util "$T")" "P0=100.0 P1=33.3 P2=0.0 P3=0.0 all=33.3"

echo "Nothing ran -> no timeline.txt"
mkdir -p "$WORK/none" && printf 'x1 = x300\n' > "$WORK/none/bad.txt" && printf '00 00 00 00\n' > "$WORK/none/d.byte"
(cd "$WORK/none" && "$SIM" -b bad.txt d.byte > run.txt 2>&1)
check "compile error: no timeline.txt" "$([ -e "$WORK/none/timeline.txt" ] && echo yes || echo no)" no

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
