#!/usr/bin/env bash
# EXT e22 checks: results stay correct when the processors run in parallel, page faults
# from several processors at once never hand out the same frame twice, and the run is
# faster with 4 threads than with 1.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s 2>&1 | grep -E "warning|error")
WORK="$(mktemp -d)"; trap 'rm -rf "$WORK"' EXIT
pass=0; fail=0
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}
check() { if [ "$2" = "$3" ]; then pass=$((pass+1)); echo "  ok    $1"; else fail=$((fail+1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi; }
cp "$EX"/* "$HERE"/heavy.txt "$HERE"/heavy_data.byte "$WORK"/
cd "$WORK"

# 4 heavy tasks, 1 thread vs 4 threads: same results, wall time printed
for t in 1 4; do
    rm -f heavy_data_*.out
    start=$(date +%s.%N 2>/dev/null || date +%s)
    OMP_NUM_THREADS=$t "$HERE/sim" -b heavy.txt heavy_data.byte heavy.txt heavy_data.byte \
        heavy.txt heavy_data.byte heavy.txt heavy_data.byte > run_$t.txt 2>&1
    end=$(date +%s.%N 2>/dev/null || date +%s)
    echo "  time with $t thread(s): $(awk -v a="$start" -v b="$end" 'BEGIN { printf "%.3f s", b - a }')"
    all=""; for pid in 1 2 3 4; do all="$all$(word_at heavy_data_$pid.out 0),"; done
    check "$t thread(s): all 4 tasks computed 6000" "$all" "6000,6000,6000,6000,"
done

# 3 tasks that page-fault at the same time, 20 runs: every result must be 21.
# Each pages.txt task needs 2 frames at load + 3 on demand = 5, so 3 tasks use exactly
# the 15 free frames (4 would not fit, and the 4th would stop with "no free frame").
bad=0
for r in $(seq 1 20); do
    rm -f pages_data_*.out
    OMP_NUM_THREADS=4 "$HERE/sim" -b pages.txt pages_data.byte pages.txt pages_data.byte \
        pages.txt pages_data.byte > pf.txt 2>&1 || bad=$((bad+1))
    for pid in 1 2 3; do [ "$(word_at pages_data_$pid.out 0)" = 21 ] || bad=$((bad+1)); done
done
check "20 runs x 3 tasks with simultaneous page faults all correct" "$bad" 0

# mixed workload in parallel (vectors, branches, prints) still correct
OMP_NUM_THREADS=4 "$HERE/sim" -b fir.txt fir_data.byte array_add.txt array_add_data.byte \
    sum_array_print.txt sum_array_data.byte counter.txt counter_data.byte > mix.txt 2>&1
check "fir in parallel" "$(word_at fir_data_1.out 316)" 744
check "array_add in parallel" "$(word_at array_add_data_2.out 192)" 131
check "sum_array in parallel" "$(word_at sum_array_data_3.out 36)" 45
check "log has every print line (9 + 10)" "$(wc -l < log.txt | tr -d ' ')" 19

echo; echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
