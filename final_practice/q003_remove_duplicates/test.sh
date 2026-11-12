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
finish
