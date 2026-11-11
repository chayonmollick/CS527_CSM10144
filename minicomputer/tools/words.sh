#!/usr/bin/env bash
# Print every 32-bit little-endian word of a .byte / .out file:  address  hex  decimal
#   tools/words.sh sum_array_data_1.out          all words
#   tools/words.sh sum_array_data_1.out 36       just the word at byte address 36
[ $# -ge 1 ] || { echo "usage: $0 <file.byte|file.out> [byte address]"; exit 1; }
awk -v ONLY="${2:-}" '
    function hex(s,  i, v) { s = toupper(s); v = 0
        for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
        return v }
    { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
    END {
        for (a = 0; a + 3 < n; a += 4) {
            if (ONLY != "" && a != ONLY + 0) continue
            v = b[a] + b[a+1] * 256 + b[a+2] * 65536 + b[a+3] * 16777216
            s = (v >= 2147483648) ? v - 4294967296 : v
            printf "%6d (0x%04X): %02X %02X %02X %02X  = %d\n", a, a, b[a], b[a+1], b[a+2], b[a+3], s
        }
    }' "$1"
