#!/usr/bin/env bash
# Q017 checks: first pair i < j with A[i] + A[j] = T, or -1 -1.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on two_sum_data (N = 8, T = 10)" "$(run_sim -b solution.txt two_sum_data.byte)" 0
check "i = 2 (A[2] = -2)" "$(word_at "$RUN/two_sum_data_1.out" 40)" 2
check "j = 5 (A[5] = 12)" "$(word_at "$RUN/two_sum_data_1.out" 44)" 5
check "runs on two_sum_data2 (T = 100, no pair)" "$(run_sim -b solution.txt two_sum_data2.byte)" 0
check "no pair: i = -1" "$(word_at "$RUN/two_sum_data2_1.out" 28)" -1
check "no pair: j = -1" "$(word_at "$RUN/two_sum_data2_1.out" 32)" -1

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
cross_check two_sum_data.byte
cross_check two_sum_data2.byte
finish
