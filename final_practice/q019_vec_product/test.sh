#!/usr/bin/env bash
# Q019 checks: C = A * B element-wise with vector blocks plus a scalar tail.
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on vec_product_data (N = 19)" "$(run_sim -b solution.txt vec_product_data.byte)" 0
check_words "$RUN/vec_product_data_1.out" 156 -20 -34 -42 -44 -40 -30 -14 8 36 70 110 156 208 266 \
    330 400 476 558 646
check "nothing written after C" "$(word_at "$RUN/vec_product_data_1.out" 232)" 0
check "the vector multiply ran 2 times (2 blocks of 8)" \
    "$(grep -F 'v1 = v1 * v2' "$RUN/solution_1.freq" | cut -f1)" 2
check "runs on vec_product_data2 (N = 5, tail only)" "$(run_sim -b solution.txt vec_product_data2.byte)" 0
check_words "$RUN/vec_product_data2_1.out" 44 -120000 63 0 -144 3000
check "nothing written after C" "$(word_at "$RUN/vec_product_data2_1.out" 64)" 0

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
cross_check vec_product_data.byte
cross_check vec_product_data2.byte
finish
