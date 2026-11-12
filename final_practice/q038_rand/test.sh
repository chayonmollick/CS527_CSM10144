#!/usr/bin/env bash
# Q038 checks: x1 = rand gives each task its own LCG sequence, seeded with its pid when it is loaded.
. "$(dirname "$0")/../lib.sh"
build_solution
seq_of() {   # the 6 numbers task <pid> stored
    local a s=""
    for a in 0 4 8 12 16 20; do s="$s $(word_at "$RUN/rnd_data_$1.out" "$a")"; done
    echo $s
}
check "5 copies of rnd run without error" \
      "$(run_sim -b rnd.txt rnd_data.byte rnd.txt rnd_data.byte rnd.txt rnd_data.byte rnd.txt rnd_data.byte rnd.txt rnd_data.byte)" 0
check "pid 1 = the C standard's example rand() after srand(1)" "$(seq_of 1)" "16838 5758 10113 17515 31051 5627"
check "pid 2 has its own sequence (seed 2), not mixed with pid 1's" "$(seq_of 2)" "908 22817 10239 12914 25837 27095"
check "pid 4 (seed 4)" "$(seq_of 4)" "1817 24166 10491 3711 15407 4496"
check "pid 5 had to wait for a processor" "$(grep -c 'pid 5 is waiting' "$RUN/run.txt")" 1
check "pid 5 reused processor 0 but restarted from seed 5" "$(seq_of 5)" "18655 8457 10616 31877 10193 25964"
rm -f "${RUN:?}/rnd_data_1.out"
check "rnd alone runs without error" "$(run_sim -b rnd.txt rnd_data.byte)" 0
check "deterministic: pid 1 alone gets the same numbers as when interleaved" "$(seq_of 1)" "16838 5758 10113 17515 31051 5627"
(cd "$RUN" && "$SIM" -c rnd.txt > listing.txt)
check "x3 = rand encodes as 8F 03 00 00" "$(grep -cF '8F 03 00 00   x3 = rand' "$RUN/listing.txt")" 1
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
