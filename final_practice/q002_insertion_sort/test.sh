#!/usr/bin/env bash
# Q002 checks: insertion sort, ascending, in place.
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on insertion_sort_data (N = 10)" "$(run_sim -b solution.txt insertion_sort_data.byte)" 0
check_words "$RUN/insertion_sort_data_1.out" 4 -61 -4 -4 0 3 5 17 29 88 300
check "runs on insertion_sort_data2 (descending)" "$(run_sim -b solution.txt insertion_sort_data2.byte)" 0
check_words "$RUN/insertion_sort_data2_1.out" 4 -10 0 10 20 30 40 50
finish
