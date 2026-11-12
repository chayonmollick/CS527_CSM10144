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
finish
