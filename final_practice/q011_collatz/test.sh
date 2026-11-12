#!/usr/bin/env bash
# Q011 checks: Collatz steps from n to 1 and the largest value reached.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on collatz_data (n = 27)" "$(run_sim -b solution.txt collatz_data.byte)" 0
check "27 needs 111 steps" "$(word_at "$RUN/collatz_data_1.out" 4)" 111
check "largest value reached = 9232" "$(word_at "$RUN/collatz_data_1.out" 8)" 9232
check "runs on collatz_data2 (n = 1)" "$(run_sim -b solution.txt collatz_data2.byte)" 0
check "1 needs 0 steps" "$(word_at "$RUN/collatz_data2_1.out" 4)" 0
check "largest value = 1" "$(word_at "$RUN/collatz_data2_1.out" 8)" 1

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
cross_check collatz_data.byte
cross_check collatz_data2.byte
finish
