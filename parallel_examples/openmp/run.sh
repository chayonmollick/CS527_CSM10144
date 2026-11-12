#!/usr/bin/env bash
# Build every example and run it with OMP_NUM_THREADS=4. An example passes when it
# exits with 0 and prints a line with the word OK (and no line with WRONG).
#   ./run.sh            all examples, one line each
#   ./run.sh 12         only the examples whose name contains "12", with their output
# Uses only bash and make; works with bash 3.2 (macOS) and later.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE" || exit 1
FILTER="${1:-}"
make -s || { echo "build failed"; exit 1; }
export OMP_NUM_THREADS=4
pass=0; fail=0

for src in [0-9][0-9]_*.c; do
    prog="${src%.c}"
    case "$prog" in *"$FILTER"*) ;; *) continue ;; esac
    out="$(./"$prog" 2>&1)"; rc=$?
    if [ -n "$FILTER" ]; then printf '%s\n' "$out" | sed 's/^/        /'; fi
    if [ "$rc" -eq 0 ] && printf '%s\n' "$out" | grep -qw OK && ! printf '%s\n' "$out" | grep -q WRONG; then
        pass=$((pass + 1)); echo "  ok    $prog"
    else
        fail=$((fail + 1)); echo "  FAIL  $prog (exit code $rc)"
        if [ -z "$FILTER" ]; then printf '%s\n' "$out" | sed 's/^/        /'; fi
    fi
done

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ] && [ "$pass" -gt 0 ]
