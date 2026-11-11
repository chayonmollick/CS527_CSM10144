#!/usr/bin/env bash
# e16 tests: ROUND_ROBIN / PRIORITY / SJF scheduling and the per-task report.
#   ./test.sh   (builds ./sim, runs everything in a temp directory)
# Jobs: job_short = 10 instructions (1 round), job_mid = 30 (3 rounds), job_long = 50 (5 rounds).
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

# report helpers (read the scheduling report printed at exit)
order()    { awk -F': ' '/finish order/ {print $2}' "$1"; }
averages() { awk '/average waiting/ {print $3, $7}' "$1"; }
table()    { awk '/^  pid program/ {on = 1; next} /finish order/ {on = 0}
                  on {printf "%s%s:%s:%s:%s:%s", sep, $1, $6, $7, $8, $9; sep = " "} END {print ""}' "$1"; }
logorder() { awk '{print $3}' "$1" | paste -sd' ' -; }

newdir() {  # newdir <name>: fresh directory with the demo jobs
    mkdir -p "$WORK/$1" && cp "$HERE"/job_*.txt "$HERE"/job_data.byte "$WORK/$1"/
}

# The six-task demo: pid 1 long p1, 2 mid p2, 3 long p3, 4 short p1, 5 mid p5, 6 short p4 (NP = 4)
TASKS="job_long.txt job_data.byte 1 job_mid.txt job_data.byte 2 job_long.txt job_data.byte 3
       job_short.txt job_data.byte 1 job_mid.txt job_data.byte 5 job_short.txt job_data.byte 4"

demo() {    # demo <policy> <finish order> <avg waiting> <avg turnaround> <table>
    local d="$WORK/$1"
    newdir "$1"
    (cd "$d" && "$SIM" -b -s "$1" $TASKS > run.txt 2>&1); local rc=$?
    check "$1: exit code" "$rc" 0
    check "$1: finish order" "$(order "$d/run.txt")" "$2"
    check "$1: average waiting and turnaround" "$(averages "$d/run.txt")" "$3 $4"
    check "$1: pid:start:finish:waiting:turnaround" "$(table "$d/run.txt")" "$5"
    check "$1: log.txt (one print per job) in finish order" "$(logorder "$d/log.txt")" "$2"
    check "$1: results 46 26 46 6 26 6" "$(for p in 1 2 3 4 5 6; do word_at "$d/job_data_$p.out" 0; done | paste -sd' ' -)" "46 26 46 6 26 6"
}

echo "Six tasks on NP = 4 processors, three policies"
demo rr       "4 2 5 6 1 3" 0.67 3.67 "1:1:5:0:5 2:1:3:0:3 3:1:5:0:5 4:1:1:0:1 5:2:4:1:4 6:4:4:3:4"
demo priority "6 5 2 4 3 1" 0.67 3.67 "1:2:6:1:6 2:1:3:0:3 3:1:5:0:5 4:4:4:3:4 5:1:3:0:3 6:1:1:0:1"
demo sjf      "4 6 2 5 1 3" 0.33 3.33 "1:2:6:1:6 2:1:3:0:3 3:2:6:1:6 4:1:1:0:1 5:1:3:0:3 6:1:1:0:1"
check "priority: first four loaded = highest priorities 5 6 3 2" \
      "$(grep 'loaded on' "$WORK/priority/run.txt" | head -4 | awk '{print $3}' | paste -sd' ' -)" "5 6 3 2"
check "sjf: first four loaded = shortest jobs 4 6 2 5" \
      "$(grep 'loaded on' "$WORK/sjf/run.txt" | head -4 | awk '{print $3}' | paste -sd' ' -)" "4 6 2 5"

echo "Default policy (config.h SCHED_POLICY = ROUND_ROBIN) behaves like Lab 4"
newdir def
(cd "$WORK/def" && "$SIM" -b $TASKS > run.txt 2>&1)
check "default policy is ROUND_ROBIN" "$(grep -c 'scheduling policy ROUND_ROBIN' "$WORK/def/run.txt")" 1
check "default: same finish order as rr" "$(order "$WORK/def/run.txt")" "4 2 5 6 1 3"

echo "Shell: '<program> <data> <priority>' (all typed while idle, so all arrive in round 0)"
SHELL_IN='job_long.txt job_data.byte 1\njob_long.txt job_data.byte 1\njob_long.txt job_data.byte 1\njob_long.txt job_data.byte 1\njob_short.txt job_data.byte 9\nexit\n'
newdir shp
(cd "$WORK/shp" && printf "$SHELL_IN" | "$SIM" -s priority > run.txt 2>&1)
check "priority via shell: urgent short job (pid 5) loaded first" "$(grep 'loaded on' "$WORK/shp/run.txt" | head -1 | awk '{print $3}')" 5
check "priority via shell: finish order" "$(order "$WORK/shp/run.txt")" "5 1 2 3 4"
check "priority via shell: pid 5 waited 0 rounds" "$(table "$WORK/shp/run.txt" | tr ' ' '\n' | grep '^5:')" "5:1:1:0:1"
newdir shr
(cd "$WORK/shr" && printf "$SHELL_IN" | "$SIM" -s rr > run.txt 2>&1)
check "round robin via shell: finish order" "$(order "$WORK/shr/run.txt")" "1 2 3 4 5"
check "round robin via shell: pid 5 waited 5 rounds" "$(table "$WORK/shr/run.txt" | tr ' ' '\n' | grep '^5:')" "5:6:6:5:6"
newdir shb
(cd "$WORK/shb" && printf 'job_short.txt job_data.byte 3\njob_short.txt job_data.byte high\nexit\n' | "$SIM" -s priority > run.txt 2>&1)
check "shell: priority 3 accepted" "$(grep -c 'pid 1 created: job_short.txt job_data.byte (10 instructions, priority 3)' "$WORK/shb/run.txt")" 1
check "shell: non-numeric priority -> one usage line" "$(grep -c 'usage' "$WORK/shb/run.txt")" 1

echo "Command-line errors"
newdir err
(cd "$WORK/err" && "$SIM" -b -s fifo job_short.txt job_data.byte > run.txt 2>&1); rc=$?
check "unknown policy -> usage, exit 1" "$rc,$(grep -c '^usage' "$WORK/err/run.txt")" "1,1"
(cd "$WORK/err" && "$SIM" -b job_short.txt job_data.byte 2 job_long.txt > run2.txt 2>&1); rc=$?
check "program without data file -> usage, exit 1" "$rc,$(grep -c '^usage' "$WORK/err/run2.txt")" "1,1"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
