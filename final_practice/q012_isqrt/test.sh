#!/usr/bin/env bash
# Q012 checks: integer square root (Newton) and the remainder n - r*r.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on isqrt_data (n = 2000000)" "$(run_sim -b solution.txt isqrt_data.byte)" 0
check "r = 1414" "$(word_at "$RUN/isqrt_data_1.out" 4)" 1414
check "n - r*r = 604" "$(word_at "$RUN/isqrt_data_1.out" 8)" 604
check "runs on isqrt_data2 (n = 9801 = 99^2)" "$(run_sim -b solution.txt isqrt_data2.byte)" 0
check "perfect square: r = 99" "$(word_at "$RUN/isqrt_data2_1.out" 4)" 99
check "perfect square: remainder 0" "$(word_at "$RUN/isqrt_data2_1.out" 8)" 0

# Cross-check with reference.cpp (the same task in C++): build it, run it on each data file and
# compare every "<address>: <value>" line it prints with the word at that address in the
# simulator's output file.
cross_check() {   # cross_check <data.byte>   (the simulator's output is $RUN/<data>_1.out)
    local ref want="" got="" addr value n=0
    if ! ref="$("$WORK/reference" "$QDIR/$1" 2>&1)" || [ -z "$ref" ]; then
        check "reference.cpp runs on $1" "${ref:-no output}" "result words"; return
    fi
    while IFS=': ' read -r addr value; do
        want="$want $addr=$value"
        got="$got $addr=$(word_at "$RUN/${1%.byte}_1.out" "$addr")"
        n=$((n + 1))
    done <<< "$ref"
    check "reference.cpp agrees with the simulator on $1 (words checked: $n)" "$got" "$want"
}
CXX="$(command -v c++ || command -v g++)"
if log="$("${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra "$QDIR/reference.cpp" -o "$WORK/reference" 2>&1)" \
    && [ -z "$log" ]; then
    check "reference.cpp builds without warnings" none none
else
    echo "$log"; check "reference.cpp builds without warnings" "warnings or errors" none
fi
cross_check isqrt_data.byte
cross_check isqrt_data2.byte
finish
