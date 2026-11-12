#!/usr/bin/env bash
# Q005 checks: the missing number of 1..N from N(N+1)/2 minus the numbers present.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on missing_number_data (N = 25)" "$(run_sim -b solution.txt missing_number_data.byte)" 0
check "missing number = 13 at 4N = 100" "$(word_at "$RUN/missing_number_data_1.out" 100)" 13
check "runs on missing_number_data2 (N = 1, empty list)" "$(run_sim -b solution.txt missing_number_data2.byte)" 0
check "missing number = 1 at 4N = 4" "$(word_at "$RUN/missing_number_data2_1.out" 4)" 1

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
cross_check missing_number_data.byte
cross_check missing_number_data2.byte
finish
