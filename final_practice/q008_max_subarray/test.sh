#!/usr/bin/env bash
# Q008 checks: Kadane's maximum subarray sum with its start and end index.
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on max_subarray_data (N = 9)" "$(run_sim -b solution.txt max_subarray_data.byte)" 0
check "max sum = 6 (4 -1 2 1)" "$(word_at "$RUN/max_subarray_data_1.out" 40)" 6
check "start index = 3" "$(word_at "$RUN/max_subarray_data_1.out" 44)" 3
check "end index = 6" "$(word_at "$RUN/max_subarray_data_1.out" 48)" 6
check "runs on max_subarray_data2 (all negative)" "$(run_sim -b solution.txt max_subarray_data2.byte)" 0
check "max sum = -2 (the largest element)" "$(word_at "$RUN/max_subarray_data2_1.out" 24)" -2
check "start index = 3" "$(word_at "$RUN/max_subarray_data2_1.out" 28)" 3
check "end index = 3" "$(word_at "$RUN/max_subarray_data2_1.out" 32)" 3

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
cross_check max_subarray_data.byte
cross_check max_subarray_data2.byte
finish
