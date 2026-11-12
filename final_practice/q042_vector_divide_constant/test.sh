#!/usr/bin/env bash
# Q042 checks: vD = vA / c divides every lane (signed, toward zero); / 0 is a compile error.
. "$(dirname "$0")/../lib.sh"
build_solution
check "vdiv runs without error" "$(run_sim -b vdiv.txt vdiv_data.byte)" 0
lanes() {   # lanes <address>: the 8 words of a vector stored there
    local i s=""
    for i in 0 1 2 3 4 5 6 7; do s="$s $(word_at "$RUN/vdiv_data_1.out" $(($1 + 4 * i)))"; done
    echo $s
}
check "v1 / 3, signed and toward zero (-100/3 = -33, -1/3 = 0)" "$(lanes 32)" "33 -33 2 -2 0 333 0 85"
check "(v1 + v1) / 2 gives v1 back" "$(lanes 64)" "100 -100 7 -7 0 1000 -1 255"
check "v1 = v1 / 255 in place" "$(lanes 96)" "0 0 0 0 0 3 0 1"
(cd "$RUN" && "$SIM" -c vdiv.txt > listing.txt)
check "v2 = v1 / 3 encodes as AC 02 01 03" "$(grep -cF 'AC 02 01 03   v2 = v1 / 3' "$RUN/listing.txt")" 1
check "v2 = v1 / 0 is a compile error" \
      "$(cd "$RUN" && "$SIM" -c vdiv_bad.txt 2>&1 | grep -cF 'vdiv_bad.txt:3: vector division by 0')" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
