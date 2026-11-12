# Shared helpers for final_practice/qNNN_*/test.sh (bash; portable awk only).
#
#   . "$(dirname "$0")/../lib.sh"
#   build_solution                         # fresh copy of ../lab6 + this question's solution.patch, built
#   run_sim -b prog.txt prog_data.byte     # runs in $RUN (the question's files + ../minicomputer/examples);
#                                          # prints the exit code; output is in $RUN/run.txt
#   check "what is checked" "$(word_at "$RUN/prog_data_1.out" 0)" 42
#   finish                                 # prints "N passed, M failed" and sets the exit code
set -u
QDIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$QDIR/../.." && pwd)"
LAB6="$ROOT/lab6"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
SIM="$WORK/sim/sim"
RUN="$WORK/run"
pass=0; fail=0

check() {   # check <name> <actual> <expected>
    if [ "$2" = "$3" ]; then pass=$((pass + 1)); echo "  ok    $1"
    else fail=$((fail + 1)); echo "  FAIL  $1: got '$2', expected '$3'"; fi
}

# Apply solution.patch (if the question has one) to a fresh copy of lab6 and build it.
build_solution() {
    cp -r "$LAB6" "$WORK/sim" && rm -f "$WORK/sim/sim" "$WORK/sim"/*.o
    if [ -f "$QDIR/solution.patch" ]; then
        if command -v patch > /dev/null; then
            (cd "$WORK/sim" && patch -s -p1 < "$QDIR/solution.patch") || { echo "solution.patch does not apply to lab6"; exit 1; }
        else
            (cd "$WORK/sim" && git apply -p1 "$QDIR/solution.patch") || { echo "solution.patch does not apply to lab6"; exit 1; }
        fi
    fi
    local out
    out="$(cd "$WORK/sim" && make 2>&1)" || { echo "$out"; echo "build failed"; exit 1; }
    if echo "$out" | grep -qE "warning|error"; then
        echo "$out" | grep -E "warning|error"; check "builds without warnings" "warnings" "none"
    else
        check "builds without warnings" "none" "none"
    fi
    mkdir -p "$RUN"
    cp "$ROOT"/minicomputer/examples/* "$RUN"/ 2> /dev/null
    for f in "$QDIR"/*.txt "$QDIR"/*.byte; do [ -f "$f" ] && cp "$f" "$RUN"/; done
    return 0
}

# Run the simulator in $RUN; prints its exit code. Output (stdout + stderr) goes to $RUN/run.txt.
run_sim() {
    (cd "$RUN" && "$SIM" "$@" > run.txt 2>&1)
    echo $?
}

# Signed 32-bit little-endian word at a byte address of a .byte/.out file.
word_at() {
    awk -v A="$2" '
        function hex(s,  i, v) { s = toupper(s); v = 0
            for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789ABCDEF", substr(s, i, 1)) - 1
            return v }
        { for (i = 1; i <= NF; i++) b[n++] = hex($i) }
        END { v = b[A] + b[A+1] * 256 + b[A+2] * 65536 + b[A+3] * 16777216
              if (v >= 2147483648) v -= 4294967296; printf "%d\n", v }' "$1"
}

finish() {
    echo
    echo "$pass passed, $fail failed"
    [ "$fail" -eq 0 ]
}
