#!/usr/bin/env bash
# e15 tests: memory-map dump (shell command "mem" and memory_map.txt snapshots).
#   ./test.sh          builds ./sim (make -s), runs 3 tasks in a scratch directory, checks the
#                      results and re-checks every snapshot of memory_map.txt with awk
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
EX="$HERE/../../examples"
(cd "$HERE" && make -s) || { echo "build failed"; exit 1; }
SIM="$HERE/sim"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
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

# consistent <map file>: "snapshots problems", checked independently of the simulator's own
# "check:" line. In every snapshot: each used frame is in exactly one page-table entry and that
# entry belongs to the same pid/processor/page as the frame line; free frames are in none;
# the "free frames:" count equals the number of free frame lines.
consistent() {
    awk '
    function finish(f) {
        if (!open) return
        for (f in used) if (cnt[f] != 1) bad++
        for (f in isfree) if (cnt[f] + 0 != 0) bad++
        if (freeline != nfree) bad++
        open = 0
    }
    /=== memory map:/ { finish(); open = 1; snaps++; nfree = 0; freeline = -1
                        split("", used); split("", isfree); split("", cnt); split("", owner); next }
    $1 == "frame" && $3 == "pid"  { used[$2] = 1; owner[$2] = $4 " " $6 " " ($7 == "instr" ? "I" : "D") $9; next }
    $1 == "frame" && $3 == "free" { isfree[$2] = 1; nfree++; next }
    $1 == "proc" && $3 == "PTBR" {        # proc Q PTBR B pid P (name): I0=f I1=f D0=f ... D7=f
        for (i = 8; i <= NF; i++) {
            split($i, kv, "=")
            if (kv[2] == "-") continue
            cnt[kv[2]]++
            if (owner[kv[2]] != $6 " " $2 " " kv[1]) bad++
        }
        next
    }
    /^free frames:/ { freeline = $3 }
    END { finish(); print snaps + 0, bad + 0 }' "$1"
}

echo "e15: 3 tasks (grow.txt grows one data page per round, pages.txt, sum_array.txt)"
mkdir -p "$WORK/three" && cp "$EX"/* "$HERE"/grow.txt "$HERE"/grow_data.byte "$WORK/three"/ && rm -f "$WORK/three/log.txt"
(cd "$WORK/three" && "$SIM" -b grow.txt grow_data.byte pages.txt pages_data.byte sum_array.txt sum_array_data.byte > run.txt 2>&1)
M="$WORK/three/memory_map.txt"
check "results: grow 1+..+7 = 28 and page 7 holds 7, pages 21, sum_array 45" \
      "$(word_at "$WORK/three/grow_data_1.out" 0),$(word_at "$WORK/three/grow_data_1.out" 3584),$(word_at \
      "$WORK/three/pages_data_2.out" 0),$(word_at "$WORK/three/sum_array_data_3.out" 36)" "28,7,21,45"
# rounds 1-8 each allocate or free a frame (grow: page p in round p, pages ends in round 2,
# grow in round 7, sum_array in round 8), plus the snapshot at exit
check "memory_map.txt: a snapshot after rounds 1..8 and one at exit" \
      "$(grep '^=== memory map:' "$M" | sed 's/=== memory map: //; s/ ===//; s/after round //' | paste -sd, -)" "1,2,3,4,5,6,7,8,at exit"
check "awk re-check: 9 snapshots, 0 problems" "$(consistent "$M")" "9 0"
check "the simulator's own check says OK in all 9 snapshots" "$(grep -c '^check: OK' "$M")" 9
# free: 15 - 3 tasks x 2 = 9; round 1: grow page 1 + pages pages 1,3,7 -> 5; round 2: grow page 2,
# pages returns 5 frames -> 9; rounds 3-6: grow takes one each; round 7: grow page 7, then it
# returns 9 frames -> 13; round 8: sum_array returns 2 -> 15
check "free frames per snapshot: 5,9,8,7,6,5,13,15,15" \
      "$(awk '/^free frames:/ { print $3 }' "$M" | paste -sd, -)" "5,9,8,7,6,5,13,15,15"
check "round 1: pid 2 (pages) has data pages 0, 1, 3, 7 mapped" \
      "$(awk '/after round 1 ===/ { s = 1 } s && $1 == "proc" && $6 == 2 {
               for (i = 8; i <= NF; i++) if ($i !~ /=-$/) printf "%s ", $i; print ""; exit }' "$M")" "I0=3 D0=4 D1=8 D3=9 D7=10 "
check "round 3: grow's pages are scattered and reuse pages.txt's old frame 3" \
      "$(awk '/after round 3 ===/ { s = 1 } s && $1 == "proc" && $6 == 1 { print $10, $11, $12, $13; exit }' "$M")" \
      "D0=2 D1=7 D2=11 D3=3"
check "at exit: all 15 frames free, all processors idle" \
      "$(awk '/at exit ===/ { s = 1 } s && /^free frames:/ { f = $3 } s && /idle/ { n++ } END { print f, n }' "$M")" "15 4"

echo "e15: the checker itself notices a corrupted map"
awk '/after round 1 ===/ { s = 1 } s { print } s && /^check:/ { exit }' "$M" | sed 's/ D1=7 / D1=11 /' > "$WORK/bad_map.txt"
# D1 of grow now points at free frame 11: frame 7 is used but unmapped, frame 11 is free but
# mapped, and frame 11's owner does not match -> 3 problems
check "corrupted snapshot (grow's D1 -> free frame 11): 3 problems" "$(consistent "$WORK/bad_map.txt")" "1 3"

echo "e15: shell command mem"
mkdir -p "$WORK/sh" && cp "$EX"/* "$WORK/sh"/
(cd "$WORK/sh" && printf 'sum_array.txt sum_array_data.byte\nmem\nexit\n' | "$SIM" > run.txt 2>&1)
check "mem prints a map with pid 1's two frames and 13 free" \
      "$(grep -c 'memory map: shell command mem' "$WORK/sh/run.txt"),$(grep -c '^frame  1  pid 1  proc 0  instr page 0' \
      "$WORK/sh/run.txt"),$(grep -c '^frame  2  pid 1  proc 0  data page 0' "$WORK/sh/run.txt"),$(grep '^free frames' \
      "$WORK/sh/run.txt" | head -1)" "1,1,1,free frames: 13 of 15"
check "mem output passes the awk check" "$(consistent "$WORK/sh/run.txt")" "1 0"
check "the task still runs to the right result" "$(word_at "$WORK/sh/sum_array_data_1.out" 36)" 45
check "'mem a b' is not the mem command: one usage message, no map" \
      "$(cd "$WORK/sh" && printf 'mem a b\nexit\n' | "$SIM" 2>&1 | grep -c 'usage\|=== memory map')" 1

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
