#!/usr/bin/env bash
# Q066 checks: getFreePage() hands out the highest free frame first; the results are identical to lab6's.
. "$(dirname "$0")/../lib.sh"
build_solution
check "sum_array + pages run without error" "$(run_sim -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte)" 0
# pid 1: code 15, data 14; pid 2: code 13, data 12, then data pages 1, 3, 7 -> 11, 10, 9 (lab6: 1 2 and 3 4 5 6 7)
check "pid 1 (sum_array) had frames 15 14" "$(grep -c '^\[os\] pid 1 released frames: 15 14$' "$RUN/run.txt")" 1
check "pid 2 (pages) had frames 13 12 11 10 9" "$(grep -c '^\[os\] pid 2 released frames: 13 12 11 10 9$' "$RUN/run.txt")" 1
check "the number of free frames is unchanged (11 after both loads)" "$(grep -c 'pid 2 (pages) loaded .* 11 frame(s) left' "$RUN/run.txt")" 1
check "pages.txt result" "$(word_at "$RUN/pages_data_2.out" 0)" 21
# the same two tasks on unmodified lab6: every output file must be byte-for-byte the same
cp -r "$LAB6" "$WORK/base" && rm -f "${WORK:?}/base/sim" "${WORK:?}/base"/*.o
(cd "$WORK/base" && make > /dev/null 2>&1) || echo "lab6 did not build"
mkdir -p "$WORK/run0" && cp "$ROOT"/minicomputer/examples/* "$WORK/run0"/
(cd "$WORK/run0" && "$WORK/base/sim" -b sum_array.txt sum_array_data.byte pages.txt pages_data.byte > run.txt 2>&1)
for f in sum_array_data_1.out pages_data_2.out sum_array_1.freq pages_2.freq; do
    check "$f is identical to lab6's" "$(cmp -s "$RUN/$f" "$WORK/run0/$f" && echo same || echo different)" same
done
run_sim -b sum_array.txt sum_array_data.byte > /dev/null
check "the old programs still work (sum_array = 45)" "$(word_at "$RUN/sum_array_data_1.out" 36)" 45
finish
