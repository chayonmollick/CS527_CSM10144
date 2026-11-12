#!/usr/bin/env bash
# Q004 checks: rotate left by k in place (three reversals), k larger than N.
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on rotate_left_data (N = 9, k = 3)" "$(run_sim -b solution.txt rotate_left_data.byte)" 0
check_words "$RUN/rotate_left_data_1.out" 8 13 0 21 -7 2 34 5 -1 8
check "runs on rotate_left_data2 (N = 6, k = 14)" "$(run_sim -b solution.txt rotate_left_data2.byte)" 0
check_words "$RUN/rotate_left_data2_1.out" 8 3 4 5 6 1 2
finish
