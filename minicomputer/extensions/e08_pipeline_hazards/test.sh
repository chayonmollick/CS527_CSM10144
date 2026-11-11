#!/usr/bin/env bash
# e08 tests: 5-stage pipeline model (RAW hazards, stalls, cycles) in <program>_<pid>.pipe
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
    mkdir -p "$dir" && cp "$EX"/* "$HERE"/hazards.txt "$HERE"/hazards_data.byte "$dir"/ &&
        (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
}

val() {     # val <pipe file> <name>: second field of the line starting with <name>
    awk -v K="$2" '$1 == K { print $2; exit }' "$1"
}

summary() { # summary <pipe file>: "instructions taken d1 d2 load_use | stalls cycles | stalls cycles"
    echo "$(val "$1" instructions) $(val "$1" taken_branches) $(val "$1" raw_hazards_d1)" \
         "$(val "$1" raw_hazards_d2) $(val "$1" load_use)" \
         "| $(awk '$1 == "without_forwarding" { print $2, $4 }' "$1")" \
         "| $(awk '$1 == "with_forwarding" { print $2, $4 }' "$1")"
}

step() {    # step <pipe file> <step>: "ID stall ID(fwd) stall" of that executed instruction
    awk -v S="$2" 'f && $1 == S { if ($NF == "bubbles") NF -= 3; print $(NF-3), $(NF-2), $(NF-1), $NF }
                   /^ *step/ { f = 1 }' "$1"
}

formula() { # formula <pipe file>: 1 if both cycle counts = 4 + n + stalls + 2 * taken
    awk '$1 == "instructions" { n = $2 } $1 == "taken_branches" { t = $2 }
         $1 ~ /_forwarding$/ { ok += ($4 == 4 + n + $2 + 2 * t) }
         END { print (ok == 2) ? 1 : 0 }' "$1"
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

echo "hazards.txt: straight-line code (10 instructions + halt)"
run hz hazards.txt hazards_data.byte
P="$WORK/hz/hazards_1.pipe"
check "11 instr, 0 taken, d1 4, d2 2, load-use 2 | no fwd 9 stalls 24 cycles | fwd 2 stalls 17 cycles" \
      "$(summary "$P")" "11 0 4 2 2 | 9 24 | 2 17"
check "I1 x2 = x1 + 5: after a load: 2 stalls / 1 stall" "$(step "$P" 2)" "5 2 4 1"
check "I3 x4 = x2 + x3: x3 at distance 1: 2 stalls / 0" "$(step "$P" 4)" "9 2 6 0"
check "I6 x7 = x4 - x5: x5 at distance 2: 1 stall / 0" "$(step "$P" 7)" "13 1 9 0"
check "I7 [4] = x7: distance 1: 2 stalls / 0" "$(step "$P" 8)" "16 2 10 0"
check "I9 [8] = x8: after a load: 2 stalls / 1 stall" "$(step "$P" 10)" "20 2 13 1"
check "I4 x5 = x1 * 2 (distance 4): no stall" "$(step "$P" 5)" "10 0 7 0"
check "cycles = 4 + instructions + stalls + 2 x taken" "$(formula "$P")" 1
check "summary line printed" "$(grep -c '^\[pipe\] pid 1 (hazards): 11 instructions, RAW hazards 6 (load-use 2), 0 taken branches, cycles 24 without forwarding, 17 with forwarding -> hazards_1.pipe$' "$WORK/hz/run.txt")" 1
check "program result [8] = 10 + 5 + 7 - 20 = 2" "$(word_at "$WORK/hz/hazards_data_1.out" 8)" 2

echo "sum_array (9 elements): per iteration x3 -> load (d1) and load -> add (load-use)"
run sa sum_array.txt sum_array_data.byte
P="$WORK/sa/sum_array_1.pipe"
check "71 instr, 10 taken, d1 20, d2 0, load-use 10 | 40 stalls 135 | 10 stalls 105" \
      "$(summary "$P")" "71 10 20 0 10 | 40 135 | 10 105"
check "first x15 = x1 - x4 waits for the load of x4" "$(step "$P" 5)" "10 2 7 1"
check "BAL: no stall, then 2 bubbles: next compare in ID at 23 / 17" "$(step "$P" 11)|$(step "$P" 12)" "20 0 14 0|23 0 17 0"
check "cycles formula" "$(formula "$P")" 1

echo "array_add (N = 16, vectors): v2 load-use and v3 -> vector store each iteration"
run aa array_add.txt array_add_data.byte
P="$WORK/aa/array_add_1.pipe"
check "31 instr, 3 taken, d1 7, d2 5, load-use 2 | 15 stalls 56 | 2 stalls 43" \
      "$(summary "$P")" "31 3 7 5 2 | 15 56 | 2 43"
check "v3 = v1 + v2 right after v2 = [x4]: 2 stalls / 1 stall" "$(step "$P" 11)" "21 2 13 1"
check "cycles formula" "$(formula "$P")" 1

echo "3 different tasks at once, then 5 x hazards (NP = 4): state is per processor and reset at load"
run mix hazards.txt hazards_data.byte sum_array.txt sum_array_data.byte array_add.txt array_add_data.byte
check "hazards (pid 1) = single run" "$(summary "$WORK/mix/hazards_1.pipe")" "11 0 4 2 2 | 9 24 | 2 17"
check "sum_array (pid 2) = single run" "$(summary "$WORK/mix/sum_array_2.pipe")" "71 10 20 0 10 | 40 135 | 10 105"
check "array_add (pid 3) = single run" "$(summary "$WORK/mix/array_add_3.pipe")" "31 3 7 5 2 | 15 56 | 2 43"
run five hazards.txt hazards_data.byte hazards.txt hazards_data.byte hazards.txt hazards_data.byte \
         hazards.txt hazards_data.byte hazards.txt hazards_data.byte
check "pid 5 had to wait for a processor" "$(grep -c 'pid 5 is waiting' "$WORK/five/run.txt")" 1
check "pid 5 (reused processor) = single run" "$(summary "$WORK/five/hazards_5.pipe")" "11 0 4 2 2 | 9 24 | 2 17"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
