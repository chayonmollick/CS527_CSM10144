#!/usr/bin/env bash
# Q044 checks: vD = [xA], xS loads lane i from xA + i*xS (strides 12, 0, -12 and 4).
. "$(dirname "$0")/../lib.sh"
build_solution
check "stride runs without error" "$(run_sim -b stride.txt stride_data.byte)" 0
lanes() {   # lanes <address>: the 8 words of a vector stored there
    local i s=""
    for i in 0 1 2 3 4 5 6 7; do s="$s $(word_at "$RUN/stride_data_1.out" $(($1 + 4 * i)))"; done
    echo $s
}
check "stride 12 from 0: column 0" "$(lanes 96)" "0 10 20 30 40 50 60 70"
check "stride 12 from 4: column 1" "$(lanes 128)" "1 11 21 31 41 51 61 71"
check "stride 12 from 8: column 2 (3 loads give the transpose)" "$(lanes 160)" "2 12 22 32 42 52 62 72"
check "stride 0: every lane = M[3][1]" "$(lanes 192)" "31 31 31 31 31 31 31 31"
check "stride -12 from M[7][2]: up column 2" "$(lanes 224)" "72 62 52 42 32 22 12 2"
check "stride 4 = the ordinary vector load" "$(lanes 256)" "0 1 2 10 11 12 20 21"
(cd "$RUN" && "$SIM" -c stride.txt > listing.txt)
check "v1 = [x1], x2 encodes as A5 01 01 02 (vD, base, stride)" "$(grep -cF 'A5 01 01 02   v1 = [x1], x2' "$RUN/listing.txt")" 1
check "a lane outside the data space stops the task" "$(run_sim -b stride_far.txt stride_data.byte)" 1
check "the error names lane 6's address, 6 * 765 = 4590" "$(grep -c 'data address 4590 is outside' "$RUN/run.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
