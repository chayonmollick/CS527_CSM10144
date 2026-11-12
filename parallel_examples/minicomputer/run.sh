#!/usr/bin/env bash
# Run the parallel mini-computer examples (a)-(e) on the lab6 simulator and check them.
#   ./run.sh                     builds ../../lab6/sim if needed, then runs every example
#   SIM=/path/to/sim ./run.sh    use another simulator build instead (no make then)
# Every run happens in a temporary directory, so no output files are left here. The
# expected answers are computed independently, straight from the input .byte files with
# awk, and compared with what the simulated tasks computed (combined by combine.sh).
# Uses only bash and portable awk.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -z "${SIM:-}" ]; then
    SIM="$HERE/../../lab6/sim"
    make -s -C "$HERE/../../lab6" || { echo "could not build the simulator"; exit 1; }
fi
[ -x "$SIM" ] || { echo "no simulator at $SIM"; exit 1; }
COMBINE="$HERE/combine.sh"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK:?}"' EXIT
pass=0; fail=0

# word_at <file> <byte address>: signed 32-bit little-endian word from a .byte/.out file
# (copied from ../../minicomputer/tests/run_tests.sh)
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

# words <file>: every word of a .byte file as a signed number, one per line (address 4k on line k+1)
words() {
    awk 'function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { for (a = 0; a + 3 < n; a += 4) { v = b[a] + b[a+1] * 256 + b[a+2] * 65536 + b[a+3] * 16777216
                  if (v >= 2147483648) v -= 4294967296; printf "%d\n", v } }' "$1"
}

# The expected answer for one input file, computed without the simulator (layouts: see the programs)
chunk_sum()  { words "$1" | awk 'NR == 1 { n = $1 } NR > 2 && NR <= n + 2 { s += $1 } END { print s + 0 }'; }
chunk_dot()  { words "$1" | awk 'NR == 1 { n = $1 } NR > 2 && NR <= n + 2 { a[NR] = $1 }
                                 NR > n + 2 && NR <= 2 * n + 2 { s += a[NR - n] * $1 } END { print s + 0 }'; }
chunk_find() { words "$1" | awk 'NR == 1 { n = $1 } NR == 3 { key = $1 } NR == 4 { start = $1 }
                                 NR > 4 && NR <= n + 4 && r == "" && $1 == key { r = start + NR - 5 }
                                 END { print (r == "" ? -1 : r) }'; }
block_y()    { words "$1" | awk 'NR == 1 { R = $1 } NR >= 10 && NR <= 17 { x[NR - 10] = $1 }
                                 NR >= 18 && NR < 18 + 8 * R { y[int((NR - 18) / 8)] += $1 * x[(NR - 18) % 8] }
                                 END { for (k = 0; k < R; k++) printf "%s%d", (k ? " " : ""), y[k]; print "" }'; }
first_found() { awk '$1 >= 0 && (f == "" || $1 < f) { f = $1 } END { print (f == "" ? -1 : f) }'; }

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

run() {     # run <dir> <sim arguments...>: fresh copy of the inputs, batch mode; prints the exit code
    local dir="$WORK/$1"; shift
    mkdir -p "$dir" && cp "$HERE"/*.txt "$HERE"/*.byte "$dir"/ && (cd "$dir" && "$SIM" -b "$@" > run.txt 2>&1)
    echo $?
}

longest() { sed -n 's/.*after \([0-9]*\) instructions.*/\1/p' "$1" | sort -n | tail -1; }   # max instructions of a task
rounds()  { echo $(( ($1 + 9) / 10 )); }                     # TIME_SLICE = 10 instructions per round

echo "(a) parallel array sum: sum_part.txt as 4 tasks, one chunk of 8 each"
D="$WORK/a"
check "4 tasks ran and exited with 0" "$(run a sum_part.txt sum1.byte sum_part.txt sum2.byte sum_part.txt sum3.byte sum_part.txt sum4.byte)" 0
exp=""; got=""; total=0
for k in 1 2 3 4; do
    e=$(chunk_sum "$HERE/sum$k.byte"); exp="$exp $e"; total=$((total + e))
    got="$got $(word_at "$D/sum${k}_$k.out" 4)"
done
check "partial sums at [4]:$exp" "$got" "$exp"
check "combine.sh sum = $total" "$("$COMBINE" sum "$D"/sum1_1.out "$D"/sum2_2.out "$D"/sum3_3.out "$D"/sum4_4.out)" "$total"
"$COMBINE" partials "$D"/sum1_1.out "$D"/sum2_2.out "$D"/sum3_3.out "$D"/sum4_4.out > "$D/partials.byte"
(cd "$D" && "$SIM" -b sum_part.txt partials.byte > final.txt 2>&1)
check "final combining task (sum_part.txt on partials.byte) = $total" "$(word_at "$D/partials_1.out" 4)" "$total"
run a1 sum_part.txt sum_all.byte > /dev/null
check "serial run, 1 task on all 32 elements (sum_all.byte) = $total" "$(word_at "$WORK/a1/sum_all_1.out" 4)" "$total"
rs=$(rounds "$(longest "$WORK/a1/run.txt")"); rp=$(rounds "$(longest "$D/run.txt")"); rc=$(rounds "$(longest "$D/final.txt")")
echo "  info  rounds of 10-instruction slices: serial $rs; parallel $rp + combining task $rc = $((rp + rc))" \
     "-> speedup $(awk -v s="$rs" -v p="$rp" -v c="$rc" 'BEGIN { printf "%.2f (%.2f without the serial combining step)", s / (p + c), s / p }')"

echo "(b) parallel dot product: dot_part.txt as 4 tasks"
D="$WORK/b"
check "4 tasks ran and exited with 0" "$(run b dot_part.txt dot1.byte dot_part.txt dot2.byte dot_part.txt dot3.byte dot_part.txt dot4.byte)" 0
exp=""; got=""; total=0
for k in 1 2 3 4; do
    e=$(chunk_dot "$HERE/dot$k.byte"); exp="$exp $e"; total=$((total + e))
    got="$got $(word_at "$D/dot${k}_$k.out" 4)"
done
check "partial dot products at [4]:$exp" "$got" "$exp"
check "combine.sh sum = A.B = $total" "$("$COMBINE" sum "$D"/dot1_1.out "$D"/dot2_2.out "$D"/dot3_3.out "$D"/dot4_4.out)" "$total"

echo "(c) parallel matrix-vector product y = A x (8x8): matvec_rows.txt as 4 tasks of 2 rows"
D="$WORK/c"
check "4 tasks ran and exited with 0" "$(run c matvec_rows.txt matvec1.byte matvec_rows.txt matvec2.byte matvec_rows.txt matvec3.byte matvec_rows.txt matvec4.byte)" 0
y=""
for k in 1 2 3 4; do y="$y${y:+ }$(block_y "$HERE/matvec$k.byte")"; done
check "combine.sh matvec: y = $y" "$("$COMBINE" matvec "$D"/matvec1_1.out "$D"/matvec2_2.out "$D"/matvec3_3.out "$D"/matvec4_4.out)" "$y"
run c1 matvec_rows.txt matvec_all.byte > /dev/null
check "serial run, 1 task with all 8 rows (matvec_all.byte), gives the same y" "$("$COMBINE" matvec "$WORK/c1/matvec_all_1.out")" "$y"
check "y computed from matvec_all.byte directly is the same" "$(block_y "$HERE/matvec_all.byte")" "$y"

echo "(d) parallel search: search_part.txt as 4 tasks"
D="$WORK/d"
check "4 tasks ran and exited with 0" "$(run d search_part.txt search1.byte search_part.txt search2.byte search_part.txt search3.byte search_part.txt search4.byte)" 0
exp=""; got=""
for k in 1 2 3 4; do exp="$exp $(chunk_find "$HERE/search$k.byte")"; got="$got $(word_at "$D/search${k}_$k.out" 4)"; done
first=$(printf '%s\n' $exp | first_found)
check "each task's first match (global index) or -1:$exp" "$got" "$exp"
check "combine.sh search: first index of the key = $first" "$("$COMBINE" search "$D"/search1_1.out "$D"/search2_2.out "$D"/search3_3.out "$D"/search4_4.out)" "$first"
D="$WORK/d2"                                    # the same chunks searched for 1000, which is not there
mkdir -p "$D" && cp "$HERE/search_part.txt" "$D"/
for k in 1 2 3 4; do awk 'NR == 3 { $0 = "E8 03 00 00" } { print }' "$HERE/search$k.byte" > "$D/search$k.byte"; done
(cd "$D" && "$SIM" -b search_part.txt search1.byte search_part.txt search2.byte search_part.txt search3.byte search_part.txt search4.byte > run.txt 2>&1)
first=$(for k in 1 2 3 4; do chunk_find "$D/search$k.byte"; done | first_found)
check "key 1000 (in no chunk): combine.sh search = $first" "$("$COMBINE" search "$D"/search1_1.out "$D"/search2_2.out "$D"/search3_3.out "$D"/search4_4.out)" "$first"

echo "(e) 5 tasks on 4 processors: sum_part.txt on sum1..sum5"
D="$WORK/e"
check "5 tasks ran and exited with 0" "$(run e sum_part.txt sum1.byte sum_part.txt sum2.byte sum_part.txt sum3.byte sum_part.txt sum4.byte sum_part.txt sum5.byte)" 0
check "pid 5 found no free processor and waited" "$(grep -c 'pid 5 is waiting' "$D/run.txt")" 1
fin1=$(grep -n 'pid 1 (sum_part) finished' "$D/run.txt" | cut -d: -f1)
load5=$(grep -n 'pid 5 (sum_part) loaded on processor 0' "$D/run.txt" | cut -d: -f1)
check "pid 5 was loaded on processor 0 after pid 1 finished there" "$([ -n "$fin1" ] && [ -n "$load5" ] && [ "$load5" -gt "$fin1" ] && echo yes)" yes
check "log.txt has 40 print lines, one per element" "$(wc -l < "$D/log.txt" | tr -d ' ')" 40
check "round robin: the first 8 log lines are pids 1,2,3,4,1,2,3,4" "$(head -8 "$D/log.txt" | awk '{ print $3 }' | paste -sd, -)" "1,2,3,4,1,2,3,4"
check "pid 5 ran alone at the end: the last 8 log lines are all pid 5" "$(tail -8 "$D/log.txt" | awk '{ print $3 }' | sort -u)" 5
total=0
for k in 1 2 3 4 5; do total=$((total + $(chunk_sum "$HERE/sum$k.byte"))); done
check "combine.sh sum of the 5 partial sums = $total" "$("$COMBINE" sum "$D"/sum1_1.out "$D"/sum2_2.out "$D"/sum3_3.out "$D"/sum4_4.out "$D"/sum5_5.out)" "$total"
r=$(rounds "$(longest "$D/run.txt")")
echo "  info  each task needs $r rounds; pid 5 can start only in round $((r + 1)), when a processor is free," \
     "and then runs alone: $((2 * r)) rounds in all (load imbalance)"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
