#!/usr/bin/env bash
# e02 tests: build, run the example programs in batch mode in a temp directory, check the results.
#   ./test.sh          (also run by "make test", followed by the 33 base checks)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
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

run() {     # run <test dir> <sim arguments...>: copy the examples there, run in batch mode, print the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && rm -f "$dir/log.txt" \
        && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

echo "e02: square.txt (one subroutine called 3 times)"
check "square exit code" "$(run sq square.txt square_data.byte)" 0
check "3^2 = 9"    "$(word_at "$WORK/sq/square_data_1.out" 12)" 9
check "12^2 = 144" "$(word_at "$WORK/sq/square_data_1.out" 16)" 144
check "25^2 = 625" "$(word_at "$WORK/sq/square_data_1.out" 20)" 625
check "square.freq: subroutine body ran 3 times" "$(cut -f1 "$WORK/sq/square_1.freq" | paste -sd, -)" "1,1,1,1,1,1,1,1,1,1,.square,3,3,.end"
check "the stack page was demand-mapped (1 page fault)" "$(grep -c 'square) finished.* 1 page fault' "$WORK/sq/run.txt")" 1
check "stack depth 1 word" "$(grep -c 'used 4 bytes of stack (lowest SP = 4092)' "$WORK/sq/run.txt")" 1

echo "e02: fact.txt (recursion with push/pop)"
check "fact exit code" "$(run fact fact.txt fact_data.byte)" 0
check "fact(5) = 120" "$(word_at "$WORK/fact/fact_data_1.out" 4)" 120
check "fact.freq: 6 calls, 5 recursive" "$(cut -f1 "$WORK/fact/fact_1.freq" | paste -sd, -)" "1,1,1,1,.fact,6,6,1,1,.recurse,5,5,5,5,5,5,.end"
check "deepest stack: 11 words = 44 bytes (6 return addresses + 5 saved n)" "$(grep -c 'used 44 bytes of stack' "$WORK/fact/run.txt")" 1
check "the stack is not written to the .out file (still 2 words)" "$(wc -l < "$WORK/fact/fact_data_1.out" | tr -d ' ')" 2
for n in 0 10 12; do
    mkdir -p "$WORK/fact$n" && printf '%02X 00 00 00\n' "$n" > "$WORK/fact$n/d.byte"
    run "fact$n" fact.txt d.byte > /dev/null
done
check "fact(0) = 1"          "$(word_at "$WORK/fact0/d_1.out" 4)" 1
check "fact(10) = 3628800"   "$(word_at "$WORK/fact10/d_1.out" 4)" 3628800
check "fact(12) = 479001600" "$(word_at "$WORK/fact12/d_1.out" 4)" 479001600

echo "e02: fib.txt (two recursive calls per level)"
check "fib exit code" "$(run fib fib.txt fib_data.byte)" 0
check "fib(10) = 55" "$(word_at "$WORK/fib/fib_data_1.out" 4)" 55
mkdir -p "$WORK/fib15" && printf '0F 00 00 00\n' > "$WORK/fib15/d.byte"
run fib15 fib.txt d.byte > /dev/null
check "fib(15) = 610" "$(word_at "$WORK/fib15/d_1.out" 4)" 610

echo "e02: three tasks with stacks at once (one SP per processor)"
check "exit code" "$(run all square.txt square_data.byte fact.txt fact_data.byte fib.txt fib_data.byte)" 0
check "square (pid 1)" "$(word_at "$WORK/all/square_data_1.out" 20)" 625
check "fact (pid 2)"   "$(word_at "$WORK/all/fact_data_2.out" 4)" 120
check "fib (pid 3)"    "$(word_at "$WORK/all/fib_data_3.out" 4)" 55

echo "e02: encodings (./sim -c fact.txt)"
mkdir -p "$WORK/enc" && cd "$WORK/enc" || exit 1
cp "$HERE/fact.txt" . && "$SIM" -c fact.txt > listing.txt 2>&1
check "call .fact (forward) = 50 00 00 03" "$(sed -n 2p program.byte)" "50 00 00 03"
check "call .fact (backward) = 50 00 00 FA" "$(sed -n 11p program.byte)" "50 00 00 FA"
check "push x1 / pop x1 / ret" "$(sed -n '9p;12p;14p' program.byte | paste -sd'|' -)" "52 00 00 01|53 01 00 00|51 00 00 00"
check "listing shows 'call -6'" "$(grep -c 'call -6' listing.txt)" 1

echo "e02: errors"
printf '00 00 00 00\n' > d.byte
printf 'x1 = 1\nret\n' > under.txt
"$SIM" -b under.txt d.byte > out.txt 2>&1; rc=$?
check "ret with an empty stack: underflow, exit code 1" "$(grep -c 'stack underflow' out.txt),$rc" "1,1"
printf 'pop x1\n' > pop.txt
check "pop with an empty stack: underflow" "$("$SIM" -b pop.txt d.byte 2>&1 | grep -c 'stack underflow')" 1
printf '.f\ncall .f\n' > endless.txt
"$SIM" -b endless.txt d.byte > out.txt 2>&1
check "endless recursion: stack overflow" "$(grep -c 'stack overflow' out.txt)" 1
check "overflow after the whole 512-byte stack" "$(grep -c 'used 512 bytes of stack (lowest SP = 3584)' out.txt)" 1
printf 'FF FF FF FF\n00 00 00 00\n' > m1.byte
check "fact(-1) never reaches 0: stack overflow" "$("$SIM" -b fact.txt m1.byte 2>&1 | grep -c 'stack overflow')" 1
printf 'x1 = 255\npush x1\nret\n' > badret.txt
check "ret to a pushed non-address is caught" "$("$SIM" -b badret.txt d.byte 2>&1 | grep -c 'ret to address 255, which is not an instruction')" 1
printf 'push v1\n' > pushv.txt
check "compile error: push needs an x register" "$("$SIM" -b pushv.txt d.byte 2>&1 | grep -c 'pushv.txt:1: usage: push x<n>')" 1
printf 'call .nowhere\n' > nolabel.txt
check "compile error: call to an unknown label" "$("$SIM" -b nolabel.txt d.byte 2>&1 | grep -c 'unknown label .nowhere')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
