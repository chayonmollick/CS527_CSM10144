#!/usr/bin/env bash
# Q015 checks: trace and anti-diagonal sum of an 8x8 matrix (results at 256 and 260).
. "$(dirname "$0")/../lib.sh"
build_solution
check "runs on diagonals_data (A[i][j] = i*j)" "$(run_sim -b solution.txt diagonals_data.byte)" 0
check "trace = 140" "$(word_at "$RUN/diagonals_data_1.out" 256)" 140
check "anti-diagonal = 56" "$(word_at "$RUN/diagonals_data_1.out" 260)" 56
check "runs on diagonals_data2 (values -300..300)" "$(run_sim -b solution.txt diagonals_data2.byte)" 0
check "trace = -275" "$(word_at "$RUN/diagonals_data2_1.out" 256)" -275
check "anti-diagonal = 185" "$(word_at "$RUN/diagonals_data2_1.out" 260)" 185

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
cross_check diagonals_data.byte
cross_check diagonals_data2.byte
finish
