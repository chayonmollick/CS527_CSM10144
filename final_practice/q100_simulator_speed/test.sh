#!/usr/bin/env bash
# Q100 checks: the simulator reports its speed (simulated instructions per second); results and
# instruction counts are the same for 1 and 4 threads and every time slice. The speeds, the
# speedups and the TIME_SLICE table are printed, never checked (they depend on the machine).
. "$(dirname "$0")/../lib.sh"
build_solution
SPIN="spin.txt spin_data.byte"
FOUR="$SPIN $SPIN $SPIN $SPIN"                          # 4 tasks x 80005 instructions
bad=0

measure() {     # measure <threads> <slice>: RATE = best of 3 runs; every run's results are checked
    local k r pid
    RATE=0
    for k in 1 2 3; do
        rm -f "$RUN"/spin_data_*.out
        OMP_NUM_THREADS=$1 run_sim -b -s "$2" $FOUR > /dev/null
        [ "$(awk '/\[os\] speed:/ { print $3 }' "$RUN/run.txt")" = 320020 ] || bad=$((bad + 1))
        for pid in 1 2 3 4; do
            [ -f "$RUN/spin_data_$pid.out" ] && [ "$(word_at "$RUN/spin_data_$pid.out" 4)" = 60000 ] || bad=$((bad + 1))
        done
        r=$(awk '/\[os\] speed:/ { print $9 }' "$RUN/run.txt")
        RATE=$(awk -v a="$RATE" -v b="$r" 'BEGIN { print (b > a) ? b : a }')
    done
}
seconds() { local TIMEFORMAT=%3R; { time run_sim "$@" > /dev/null; } 2>&1; }

run_sim -b $FOUR > /dev/null                           # warm-up (first start loads libgomp)
check "the default time slice is 10" "$(grep -c 'time slice 10)' "$RUN/run.txt")" 1
check "-s 0 is rejected" "$(run_sim -b -s 0 $SPIN)" 1

echo "  TIME_SLICE experiment: 4 tasks x 80005 instructions, best of 3 runs, machine-specific"
echo "      slice   1 thread  4 threads   speedup    (million simulated instructions per second)"
for s in 10 100 1000 10000; do
    measure 1 "$s"; r1=$RATE
    measure 4 "$s"; r4=$RATE
    awk -v s="$s" -v a="$r1" -v b="$r4" 'BEGIN { printf "    %7d  %9.2f  %9.2f  %8.2f\n", s, a, b, b / a }'
done
check "all 24 runs: every task stored 60000, 320020 instructions in all" "$bad" 0

# Is the limit the computer or the program? The same work as 4 separate processes, which share
# no memory, against 4 threads of one process (time slice 10000, so the work dominates).
BIG="spin.txt big_data.byte"
for k in 1 2 3 4; do mkdir -p "$RUN/p$k" && cp "$RUN/spin.txt" "$RUN/big_data.byte" "$RUN/p$k"/; done
a=$(OMP_NUM_THREADS=1 seconds -b -s 10000 $BIG $BIG $BIG $BIG)
b=$(OMP_NUM_THREADS=4 seconds -b -s 10000 $BIG $BIG $BIG $BIG)
check "4 big tasks with 4 threads: each stored 1500000" \
    "$(for pid in 1 2 3 4; do printf '%s,' "$(word_at "$RUN/big_data_$pid.out" 4)"; done)" "1500000,1500000,1500000,1500000,"
c=$( { TIMEFORMAT=%3R; time {
    for k in 1 2 3 4; do (cd "$RUN/p$k" && OMP_NUM_THREADS=1 "$SIM" -b -s 10000 $BIG > run.txt 2>&1) & done
    wait; }; } 2>&1 )
check "4 separate processes: each stored 1500000" \
    "$(for k in 1 2 3 4; do printf '%s,' "$(word_at "$RUN/p$k/big_data_1.out" 4)"; done)" "1500000,1500000,1500000,1500000,"
echo "  time  4 x 2000005 instructions, slice 10000: 1 thread $a s, 4 threads $b s, 4 processes $c s"

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
