#!/usr/bin/env bash
# Q014 checks: row N of Pascal's triangle built with vector adds (right-to-left blocks).
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on pascal_row_data (N = 10)" "$(run_sim -b solution.txt pascal_row_data.byte)" 0
check_words "$RUN/pascal_row_data_1.out" 8 1 10 45 120 210 252 210 120 45 10 1
check "word after C(10,10) is 0" "$(word_at "$RUN/pascal_row_data_1.out" 52)" 0
check "runs on pascal_row_data2 (N = 33)" "$(run_sim -b solution.txt pascal_row_data2.byte)" 0
check_words "$RUN/pascal_row_data2_1.out" 8 1 33 528 5456 40920 237336 1107568 4272048 13884156 \
    38567100 92561040 193536720 354817320 573166440 818809200 1037158320 1166803110 1166803110 \
    1037158320 818809200 573166440 354817320 193536720 92561040 38567100 13884156 4272048 1107568 \
    237336 40920 5456 528 33 1
check "word after C(33,33) is 0" "$(word_at "$RUN/pascal_row_data2_1.out" 144)" 0

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
cross_check pascal_row_data.byte
cross_check pascal_row_data2.byte
finish
