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
finish
