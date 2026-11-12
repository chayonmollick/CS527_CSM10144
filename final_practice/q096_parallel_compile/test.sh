#!/usr/bin/env bash
# Q096 checks: the programs on the command line are compiled at the same time, yet the pids,
# the messages, program.byte, every .out/.freq file and log.txt are identical to the unmodified
# lab6, with 1 and with 4 threads. Then 40 large programs: compile time printed, not checked.
. "$(dirname "$0")/../lib.sh"
build_solution
cp -r "$LAB6" "$WORK/ref" && rm -f "$WORK/ref/sim" "$WORK/ref"/*.o      # the unmodified lab6
(cd "$WORK/ref" && make > /dev/null 2>&1) || { echo "lab6 reference build failed"; exit 1; }
(cd "$RUN" && "$SIM" -c sum_array.txt sum.byte > /dev/null)           # a precompiled program

# A compile error in the middle; the last program is a .byte file, so program.byte must hold
# the last *source* program (complex_mul), as in the serial version.
LIST="fir.txt fir_data.byte array_add.txt array_add_data.byte broken.txt counter_data.byte"
LIST="$LIST sum_array.txt sum_array_data.byte counter.txt counter_data.byte det3x3.txt det3x3_data.byte"
LIST="$LIST complex_mul.txt complex_mul_data.byte sum.byte sum_array_data.byte"
run_in() {      # run_in <name> <sim> <threads>: run LIST in a fresh folder $WORK/<name>
    mkdir -p "$WORK/$1" && cp "$RUN"/*.txt "$RUN"/*.byte "$WORK/$1"/
    (cd "$WORK/$1" && OMP_NUM_THREADS=$3 "$2" -b $LIST > stdout.txt 2> stderr.txt)
    echo $?
}
same() {        # same <a> <b>: "same" if every result file and the output match, else the first difference
    local f
    for f in stdout.txt program.byte log.txt $(cd "$WORK/$1" && ls *.out *.freq); do
        cmp -s "$WORK/$1/$f" "$WORK/$2/$f" || { echo "$f differs"; return; }
    done
    grep -v 'compiled in' "$WORK/$2/stderr.txt" | cmp -s "$WORK/$1/stderr.txt" - || { echo "stderr differs"; return; }
    echo same
}
run_in ref "$WORK/ref/sim" 1 > /dev/null
for t in 1 4; do
    check "$t thread(s): exit code 1 (one program does not compile), as lab6" "$(run_in t$t "$SIM" "$t")" 1
    check "$t thread(s): output, program.byte, .out, .freq, log.txt same as lab6" "$(same ref t$t)" same
    echo "  time  $(grep 'compiled in' "$WORK/t$t/stderr.txt")"
done
check "4 threads: messages in command-line order, the error in its place" \
    "$(grep -oE 'pid [0-9]+ created: [a-z_0-9]+|cannot start task: broken.txt:4' "$WORK/t4/stdout.txt" | sed 's/.*created: //' | paste -sd, -)" \
    "fir,array_add,cannot start task: broken.txt:4,sum_array,counter,det3x3,complex_mul,sum"
check "the tasks after it keep their pids (sum_array is pid 3)" "$(grep -c 'pid 3 created: sum_array.txt' "$WORK/t4/stdout.txt")" 1
(cd "$RUN" && "$SIM" -c complex_mul.txt expected.byte > /dev/null)
check "program.byte = the last source program (complex_mul)" "$(cmp -s "$RUN/expected.byte" "$WORK/t4/program.byte" && echo yes)" yes
check "results: fir 744, array_add 131, sum.byte 45" \
    "$(word_at "$WORK/t4/fir_data_1.out" 316) $(word_at "$WORK/t4/array_add_data_2.out" 192) $(word_at "$WORK/t4/sum_array_data_7.out" 36)" \
    "744 131 45"

# 40 large programs (252 statements each), where compiling takes measurable time.
awk 'BEGIN { print "x1 = 0"; for (i = 0; i < 250; i++) print "x1 = x1 + 1"; print "[0] = x1" }' > "$RUN/big.txt"
printf '00 00 00 00\n' > "$RUN/big_data.byte"
BIG=""; for k in $(seq 1 40); do BIG="$BIG big.txt big_data.byte"; done
run_sim -b $BIG > /dev/null                           # warm-up (first start loads libgomp)
for t in 1 4; do
    rm -f "$RUN"/big_data_*.out
    OMP_NUM_THREADS=$t run_sim -b $BIG > /dev/null
    echo "  time  $(grep 'compiled in' "$RUN/run.txt") (machine-specific, not checked)"
    check "$t thread(s): 40 big tasks finished, the last stored 250" \
        "$(grep -c 'finished on' "$RUN/run.txt") $(word_at "$RUN/big_data_40.out" 0)" "40 250"
done

run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
