#!/usr/bin/env bash
# Q007 checks: integer average and the count of elements above it.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on above_average_data (N = 10)" "$(run_sim -b solution.txt above_average_data.byte)" 0
check "average 150 / 10 = 15" "$(word_at "$RUN/above_average_data_1.out" 44)" 15
check "3 elements > 15 (15 itself not counted)" "$(word_at "$RUN/above_average_data_1.out" 48)" 3
check "runs on above_average_data2 (negative sum)" "$(run_sim -b solution.txt above_average_data2.byte)" 0
check "average -27 / 5 = -5 (towards zero)" "$(word_at "$RUN/above_average_data2_1.out" 24)" -5
check "2 elements > -5" "$(word_at "$RUN/above_average_data2_1.out" 28)" 2

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
cross_check above_average_data.byte
cross_check above_average_data2.byte
finish
