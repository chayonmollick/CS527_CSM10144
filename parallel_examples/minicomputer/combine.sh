#!/usr/bin/env bash
# combine.sh - the "gather / reduce" step of a parallel run on the simulator, done on the host.
# The tasks cannot share memory in lab6 (each task has its own page table and frames),
# so each task leaves its result in its own output file <data>_<pid>.out; this script
# reads those files and combines the results. Uses only bash and portable awk.
#
#   ./combine.sh sum      sum1_1.out sum2_2.out ...   add up the words at [4]  (sum, dot product)
#   ./combine.sh search   search1_1.out ...           smallest [4] that is not -1 (first match), else -1
#   ./combine.sh matvec   matvec1_1.out ...           every block's y values, in the order given
#   ./combine.sh partials sum1_1.out ... > p.byte     a data file for a final combining TASK:
#                                                     N = number of files, 0, then each file's [4]
#                                                     (run it with: sim -b sum_part.txt p.byte)
set -u

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

# tobyte <value>...: one line per value, a 32-bit little-endian word (10 -> "0A 00 00 00")
tobyte() {
    for v in "$@"; do
        x=$(( v & 0xFFFFFFFF ))
        printf "%02X %02X %02X %02X\n" $(( x & 255 )) $(( x >> 8 & 255 )) $(( x >> 16 & 255 )) $(( x >> 24 & 255 ))
    done
}

usage() { echo "usage: $0 sum|search|matvec|partials <task output files (.out)...>" >&2; exit 1; }

[ $# -ge 2 ] || usage
mode="$1"; shift
for f in "$@"; do [ -f "$f" ] || { echo "$0: no such file: $f" >&2; exit 1; }; done

case "$mode" in
    sum)
        total=0
        for f in "$@"; do total=$(( total + $(word_at "$f" 4) )); done
        echo "$total" ;;
    search)
        first=-1
        for f in "$@"; do
            r=$(word_at "$f" 4)
            if [ "$r" -ge 0 ] && { [ "$first" -lt 0 ] || [ "$r" -lt "$first" ]; }; then first=$r; fi
        done
        echo "$first" ;;
    matvec)
        y=""
        for f in "$@"; do
            R=$(word_at "$f" 0)                         # rows in this block; its y starts at 68 + 32R
            k=0
            while [ "$k" -lt "$R" ]; do
                y="$y${y:+ }$(word_at "$f" $(( 68 + 32 * R + 4 * k )))"
                k=$(( k + 1 ))
            done
        done
        echo "$y" ;;
    partials)
        tobyte "$#" 0
        for f in "$@"; do tobyte "$(word_at "$f" 4)"; done ;;
    *)
        usage ;;
esac
