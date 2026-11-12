#!/usr/bin/env bash
# Q020 checks: window-3 moving average, vector sums + scalar division.
. "$(dirname "$0")/../lib.sh"
check_words() {   # check_words <out file> <first address> <expected words...>
    local f="$1" a="$2" v; shift 2
    for v in "$@"; do check "[$a] = $v" "$(word_at "$f" "$a")" "$v"; a=$((a + 4)); done
}
build_solution
check "runs on moving_average_data (N = 13)" "$(run_sim -b solution.txt moving_average_data.byte)" 0
check_words "$RUN/moving_average_data_1.out" 56 6 9 6 2 0 3 5 13 9 12 4
check "only N - 2 outputs" "$(word_at "$RUN/moving_average_data_1.out" 100)" 0
check "one vector block of 8 sums" "$(grep -F 'v1 = v1 + v3' "$RUN/solution_1.freq" | cut -f1)" 1
check "runs on moving_average_data2 (N = 5, negative sums)" "$(run_sim -b solution.txt moving_average_data2.byte)" 0
check_words "$RUN/moving_average_data2_1.out" 24 -2 0 -2
check "only N - 2 outputs" "$(word_at "$RUN/moving_average_data2_1.out" 36)" 0
finish
