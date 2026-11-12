#!/usr/bin/env bash
# Run every matrix program in this folder on the simulator and check its results.
#   ./run.sh                     all programs (first builds ../../lab6/sim if needed)
#   ./run.sh rotate              only the programs whose name contains "rotate"
#   SIM=~/exam/sim/sim ./run.sh  check another simulator build instead (no make then)
#   CXX=g++ ./run.sh             choose the C++ compiler for the references
# For every <name>.txt and each of its data files <name>_data*.byte:
#   1. run it with "sim -b" in a fresh scratch directory: it must finish without an error;
#   2. build <name>_ref.cpp (the same task in plain C++17), run it on the same data file,
#      and check every "<byte address>: <value>" line it prints against the word at that
#      address in the task's final memory (<data>_1.out);
#   3. at the end, check values worked out by hand (independent of the references).
# Nothing is written in this folder. Uses only bash, a C++ compiler and portable awk.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
FILTER="${1:-}"
if [ -z "${SIM:-}" ]; then
    SIM="$HERE/../../lab6/sim"
    make -s -C "$HERE/../../lab6" || { echo "could not build the simulator"; exit 1; }
fi
[ -x "$SIM" ] || { echo "no simulator at $SIM"; exit 1; }
if [ -z "${CXX:-}" ]; then
    if command -v c++ > /dev/null 2>&1; then CXX=c++; else CXX=g++; fi
fi
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK:?}"' EXIT
pass=0; fail=0

# word_at <file> <byte address>: signed 32-bit little-endian word from a .byte/.out file
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

# 1 + 2: every program, every data file, against its C++ reference
for src in "$HERE"/*.txt; do
    name="$(basename "$src" .txt)"
    case "$name" in *"$FILTER"*) ;; *) continue ;; esac
    echo "$name"
    ref="$WORK/$name.ref"
    "$CXX" -std=c++17 -O2 -Wall -Wextra -o "$ref" "$HERE/${name}_ref.cpp" > "$WORK/$name.cc.txt" 2>&1
    rc=$?
    if [ "$rc" -ne 0 ] || [ -s "$WORK/$name.cc.txt" ]; then
        check "${name}_ref.cpp compiles without warnings" "exit $rc, $(wc -l < "$WORK/$name.cc.txt" | tr -d ' ') lines of messages" "exit 0, 0 lines of messages"
        sed 's/^/          /' "$WORK/$name.cc.txt" | head -6
        continue
    fi
    for data in "$HERE/${name}"_data*.byte; do
        d="$(basename "$data" .byte)"
        dir="$WORK/$d"
        mkdir -p "$dir" && cp "$src" "$data" "$dir"/
        (cd "$dir" && "$SIM" -b "$name.txt" "$d.byte" > run.txt 2>&1)
        rc=$?
        check "$d.byte compiles and runs to the end" "$rc" 0
        [ "$rc" -eq 0 ] || { sed 's/^/          /' "$dir/run.txt" | head -6; continue; }
        out="$dir/${d}_1.out"
        "$ref" "$data" > "$dir/expected.txt" || { check "${name}_ref runs on $d.byte" "failed" "ok"; continue; }
        n=0; bad=0
        while read -r line; do
            addr="${line%%:*}"; want="${line#*: }"
            got="$(word_at "$out" "$addr")"
            n=$((n + 1))
            if [ "$got" != "$want" ]; then
                bad=$((bad + 1))
                [ "$bad" -le 5 ] && echo "          [$addr] = $got, the reference says $want"
            fi
        done < "$dir/expected.txt"
        if [ "$n" -gt 0 ]; then
            check "$d.byte: $n result words equal ${name}_ref.cpp" "$bad wrong" "0 wrong"
        else
            check "$d.byte: ${name}_ref.cpp printed result words" "none" "some"
        fi
    done
done

# 3: values worked out by hand. hand <data file without .byte> <what> <addr>=<value> ...
hand() {
    local d="$1" what="$2"; shift 2
    local out="$WORK/$d/${d}_1.out" got="" want="" kv
    [ -f "$out" ] || return 0                # filtered out, or it did not run (already failed)
    for kv in "$@"; do
        got="$got $(word_at "$out" "${kv%%=*}")"
        want="$want ${kv#*=}"
    done
    check "by hand: $d $what" "${got# }" "${want# }"
}
echo "values worked out by hand"
hand scalar_mul_vec_data  "3A: first, last of the block, first and last of the tail" 72=6 100=-6 104=24 128=-9
hand scalar_mul_vec_data2 "-2A: A = 1..16"                       76=-2 104=-16 136=-32
hand mat_sub_vec_data     "C = A - B corners, D = 2A - 3B"       128=4 136=-6 160=12 188=3 192=7 204=22 252=-1
hand hadamard_vec_data    "H[0][0], H[7][7], column sums, S"     512=6 764=-3 768=7 796=-4 800=30
hand col_sums_vec_data    "S[0], S[1], S[7]"                     164=110 168=-24 192=92
hand col_sums_vec_data2   "A[i][j] = 8i + j: S[j] = 224 + 8j"    260=224 264=232 288=280
hand sum_all_vec_data     "sum 231 - 2*60, mean 111/21"          92=111 96=5
hand sum_all_vec_data2    "negative sum, mean rounds towards 0"  72=-23 76=-1
hand row_sums_data        "sums 7 18 11, max 18 in row 1"        56=7 60=18 64=11 68=18 72=1
hand row_sums_data2       "sums -3 -3 -2 -3, max -2 in row 2"    40=-3 44=-3 48=-2 52=-3 56=-2 60=2
hand max_elem_data        "15, first at (0,2)"                   56=15 60=0 64=2
hand max_elem_data2       "-3, first at (1,0)"                   48=-3 52=1 56=0
hand count_zeros_data     "15 zeros, 5 nonzeros, 2 zero rows"    88=15 92=5 96=2
hand count_zeros_data2    "no zeros"                             44=0 48=9 52=0
hand is_identity_data     "I4 -> 1"                              68=1
hand is_identity_data2    "A[3][2] = 1 -> 0"                     68=0
hand is_identity_data3    "diag(1,2,1) -> 0"                     40=0
hand is_upper_data        "upper triangular"                     68=1 72=0
hand is_upper_data2       "two nonzeros below"                   68=0 72=2
hand transpose_inplace_data  "A = 1..16: T[0][1] = 5, T[1][0] = 2, T[2][2] = 11, T[3][3] = 16" 8=5 20=2 44=11 64=16
hand transpose_inplace_data2 "3x3 with negatives"                8=4 16=-2 32=-6 36=9
hand rotate90_data        "row 0 = 13 9 5 1, row 3 = 16 12 8 4"  4=13 8=9 16=1 52=16 64=4
hand rotate90_data2       "7 4 1 / 8 5 2 / 9 6 3"                4=7 12=1 20=5 28=9 36=3
hand mat_square_data      "C[0][0], C[0][3], C[2][2], C[3][3]"   68=5 80=-2 108=9 128=11
hand mat_square_data2     "[[1,1],[1,0]]^2 = [[2,1],[1,1]]"      20=2 24=1 28=1 32=1
hand det_adj2x2_data      "det 10, adj, flag, A*adj = 10I"       16=10 20=6 24=-7 28=-2 32=4 36=1 40=10 44=0 48=0 52=10
hand det_adj2x2_data2     "singular: det 0, flag 0, A*adj = 0"   16=0 20=6 24=3 28=4 32=2 36=0 40=0 52=0

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
