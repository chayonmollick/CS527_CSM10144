#!/usr/bin/env bash
# Q009 checks: median of an odd-length array by counting ranks; the array is unchanged.
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on median_data (N = 9)" "$(run_sim -b solution.txt median_data.byte)" 0
check "median = 15" "$(word_at "$RUN/median_data_1.out" 40)" 15
check_words "$RUN/median_data_1.out" 4 42 -7 15 3 15 99 0 8 23
check "runs on median_data2 (-3 three times)" "$(run_sim -b solution.txt median_data2.byte)" 0
check "median = -3" "$(word_at "$RUN/median_data2_1.out" 24)" -3
finish
