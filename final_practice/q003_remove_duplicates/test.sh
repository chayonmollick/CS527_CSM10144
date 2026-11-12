#!/usr/bin/env bash
# Q003 checks: duplicates removed from a sorted array in place, new length stored.
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on remove_duplicates_data (N = 12)" "$(run_sim -b solution.txt remove_duplicates_data.byte)" 0
check_words "$RUN/remove_duplicates_data_1.out" 4 -5 -2 0 3 7 9
check "new length L = 6 at 4N + 4" "$(word_at "$RUN/remove_duplicates_data_1.out" 52)" 6
check "runs on remove_duplicates_data2 (all 4)" "$(run_sim -b solution.txt remove_duplicates_data2.byte)" 0
check "A[0] = 4" "$(word_at "$RUN/remove_duplicates_data2_1.out" 4)" 4
check "new length L = 1" "$(word_at "$RUN/remove_duplicates_data2_1.out" 24)" 1

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
cross_check remove_duplicates_data.byte
cross_check remove_duplicates_data2.byte
finish
