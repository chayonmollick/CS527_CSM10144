#!/usr/bin/env bash
# Q056 checks: <program>_<pid>.dump holds the final data memory as "[address] = decimal word".
. "$(dirname "$0")/../lib.sh"
build_solution
check "sum_array, sum_n and pages run together" "$(run_sim -b sum_array.txt sum_array_data.byte sum_n.txt sum_n_data.byte pages.txt pages_data.byte)" 0
D="$RUN/sum_array_1.dump"
check "sum_array: 10 words, one line each" "$(wc -l < "$D" | tr -d ' ')" 10
check "first line: the array size" "$(head -1 "$D")" "[0] = 9"
check "last line: the sum, stored over the last element" "$(tail -1 "$D")" "[36] = 45"
same=yes
while read -r addr _ value; do
    [ "$(word_at "$RUN/sum_array_data_1.out" "${addr//[^0-9]/}")" = "$value" ] || same=no
done < "$D"
check "every line equals that word of sum_array_data_1.out" "$same" yes
check "sum_n: a negative word in decimal" "$(sed -n 3p "$RUN/sum_n_2.dump")" "[8] = -1"
check "sum_n: the sum at 4N + 4" "$(tail -1 "$RUN/sum_n_2.dump")" "[24] = 99"
check "pages: words up to the store at 3825: 3832 / 4 = 958 lines" "$(wc -l < "$RUN/pages_3.dump" | tr -d ' ')" 958
check "pages: the 7 stored at 3825 is 7 x 256 in the word at 3824" "$(grep -F '[3824] = ' "$RUN/pages_3.dump")" "[3824] = 1792"
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
