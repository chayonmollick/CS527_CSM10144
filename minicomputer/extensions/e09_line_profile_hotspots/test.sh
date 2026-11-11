#!/usr/bin/env bash
# e09 tests: line profile with estimated cycles, top-3 hottest lines and dead code (<program>_<pid>.prof)
#   ./test.sh          builds ./sim (make -s), runs the programs in a scratch directory
# Expected numbers are derived by hand in README.md ("Expected output").
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
SIM="$HERE/sim"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <test dir> <sim arguments...>   (batch mode, fresh directory)
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/profile.txt "$HERE"/profile_data.byte "$dir"/ &&
        (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
}

line() {    # line <prof file> <line number or ->: "count cycles %cycles" from the main table
    awk -v L="$2" '/^top / { exit } NR > 2 && $1 == L { print $2, $3, $4; exit }' "$1"
}

top() {     # top <prof file>: "line:cycles:%" of the hottest lines, comma separated
    awk '/^top / { f = 1; getline; next } f && NF == 0 { exit }
         f { printf "%s%s:%s:%s", (n++ ? "," : ""), $2, $3, $4 } END { print "" }' "$1"
}

dead() {    # dead <prof file>: "<count>|<line numbers>"
    awk '/^never executed/ { c = $(NF-1); f = 1; next } f { s = s (s ? " " : "") $1 } END { print c "|" s }' "$1"
}

sumcycles() {   # sum of the cycles column of the main table (including the halt row)
    awk '/^top / { exit } NR > 2 && NF >= 4 && $2 ~ /^[0-9]+$/ { s += $3 } END { print s }' "$1"
}

word_at() { # word_at <file> <byte address>: signed 32-bit little-endian word from a .out file
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

echo "profile.txt: loop of 6 with a DIV (10 cycles), 2 lines of dead code after BAL"
run pr profile.txt profile_data.byte
P="$WORK/pr/profile_1.prof"
check "header: 41 instructions, 121 cycles" "$(sed -n 1p "$P" | sed 's/.*: //')" "41 instructions executed, 121 estimated cycles"
check "line 3 x1 = 0: once, 1 cycle" "$(line "$P" 3)" "1 1 0.83"
check "line 6 load: 6 x 3 = 18" "$(line "$P" 6)" "6 18 14.88"
check "line 7 div: 6 x 10 = 60" "$(line "$P" 7)" "6 60 49.59"
check "line 8 add: 6 x 1" "$(line "$P" 8)" "6 6 4.96"
check "line 11 BNE: 5 x 3 + 1 x 1 = 16" "$(line "$P" 11)" "6 16 13.22"
check "line 12 BAL: taken once = 3" "$(line "$P" 12)" "1 3 2.48"
check "line 13 never executed" "$(line "$P" 13)" "0 0 0.00"
check "line 16 store: 3" "$(line "$P" 16)" "1 3 2.48"
check "halt row" "$(line "$P" -)" "1 1 0.83"
check "cycles column adds up to 121" "$(sumcycles "$P")" 121
check "top 3: line 7 (60), line 6 (18), line 11 (16)" "$(top "$P")" "7:60:49.59,6:18:14.88,11:16:13.22"
check "dead code: lines 13 and 14" "$(dead "$P")" "2|13 14"
check ".freq file unchanged (Lab 6 format)" "$(cut -f1 "$WORK/pr/profile_1.freq" | paste -sd, -)" "1,1,.loop,6,6,6,6,6,6,1,0,0,.done,1"
check "summary line printed" "$(grep -c '^\[prof\] pid 1 (profile): 121 cycles, hottest line 7 (49.59%), 2 never executed -> profile_1.prof$' "$WORK/pr/run.txt")" 1
check "program result [4] = 0+0+1+1+2+2 = 6" "$(word_at "$WORK/pr/profile_data_1.out" 4)" 6

echo "sum_array (9 elements): load and BAL tie at 27 cycles (tie -> earlier line first)"
run sa sum_array.txt sum_array_data.byte sum_array_print.txt sum_array_data.byte
P="$WORK/sa/sum_array_1.prof"
check "line 7 BEQ: 9 x 1 + 1 x 3 = 12" "$(line "$P" 7)" "10 12 10.62"
check "line 9 load: 9 x 3 = 27" "$(line "$P" 9)" "9 27 23.89"
check "line 12 BAL: 9 x 3 = 27" "$(line "$P" 12)" "9 27 23.89"
check "cycles column adds up to 113" "$(sumcycles "$P")" 113
check "top 3: line 9, line 12, line 7" "$(top "$P")" "9:27:23.89,12:27:23.89,7:12:10.62"
check "no dead code" "$(dead "$P")" "0|"
check "sum_array_print .freq = handout example" "$(cut -f1 "$WORK/sa/sum_array_print_2.freq" | paste -sd, -)" \
      "1,1,1,1,.loopback,10,10,9,9,9,9,9,9,.exit,1"
check "sum_array_print: print line 9 x 1 cycle, total 113 + 9" "$(line "$WORK/sa/sum_array_print_2.prof" 12)|$(sumcycles "$WORK/sa/sum_array_print_2.prof")" "9 9 7.38|122"

echo "precompiled .byte program (no source): one row per instruction"
mkdir -p "$WORK/byte" && cp "$HERE"/profile.txt "$HERE"/profile_data.byte "$WORK/byte"/
(cd "$WORK/byte" && "$SIM" -c profile.txt prof.byte > /dev/null && "$SIM" -b prof.byte profile_data.byte > run.txt 2>&1)
P="$WORK/byte/prof_1.prof"
check "top 3 by instruction index: 3, 2, 7" "$(top "$P")" "3:60:49.59,2:18:14.88,7:16:13.22"
check "never executed: instructions 9 and 10" "$(dead "$P")" "2|9 10"

echo "5 tasks at once (NP = 4): cycle counts are per processor and cleared at load"
run five profile.txt profile_data.byte sum_array.txt sum_array_data.byte profile.txt profile_data.byte \
         profile.txt profile_data.byte profile.txt profile_data.byte
check "pid 5 had to wait for a processor" "$(grep -c 'pid 5 is waiting' "$WORK/five/run.txt")" 1
check "sum_array (pid 2) = single run" "$(top "$WORK/five/sum_array_2.prof")" "9:27:23.89,12:27:23.89,7:12:10.62"
for pid in 1 3 4 5; do
    check "profile pid $pid = single run" "$(sumcycles "$WORK/five/profile_$pid.prof") $(dead "$WORK/five/profile_$pid.prof")" "121 2|13 14"
done

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
