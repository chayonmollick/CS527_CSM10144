#!/usr/bin/env bash
# Q016 checks: mode of values 0..15 (ties -> smallest) and its count.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on mode_data (N = 12)" "$(run_sim -b solution.txt mode_data.byte)" 0
check "tie between 3 and 7 -> mode 3" "$(word_at "$RUN/mode_data_1.out" 52)" 3
check "count = 3" "$(word_at "$RUN/mode_data_1.out" 56)" 3
check "runs on mode_data2 (N = 9)" "$(run_sim -b solution.txt mode_data2.byte)" 0
check "mode = 15 (the top counter)" "$(word_at "$RUN/mode_data2_1.out" 40)" 15
check "count = 4" "$(word_at "$RUN/mode_data2_1.out" 44)" 4

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
cross_check mode_data.byte
cross_check mode_data2.byte
finish
