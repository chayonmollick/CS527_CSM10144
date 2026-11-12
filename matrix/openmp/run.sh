#!/usr/bin/env bash
# Build and check every OpenMP matrix program in this folder.
#   ./run.sh              all programs
#   ./run.sh m04          only the programs whose name contains "m04"
#   CC=gcc-9 ./run.sh     use another compiler (passed on to the Makefile)
# Each program is built through the Makefile into a temporary directory, so nothing
# is left in this folder. A program passes when it compiles without any warning,
# exits with status 0, prints at least one "OK" and no "WRONG". The sizes below are
# small so the whole run takes a few seconds; the defaults (no arguments) are for timing.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
FILTER="${1:-}"
WORK="$(mktemp -d)"
trap 'rm -rf "${WORK:?}"' EXIT
pass=0; fail=0

# Small arguments for each program.
args_for() {
    case "$1" in
        m01_*) echo "8 7 6 5" ;;      # A 8x7, B 6x5
        m02_*) echo "300" ;;
        m03_*) echo "200 50" ;;       # A 200x50
        m04_*) echo "150" ;;
        m05_*) echo "300 8" ;;        # N = 300, 8 right-hand sides
        m06_*) echo "200" ;;
        m07_*) echo "203 16" ;;       # partial edge tiles
        m08_*) echo "5000 40000" ;;
        *)     echo "" ;;
    esac
}

for src in "$HERE"/m[0-9]*.c; do
    name="$(basename "$src" .c)"
    case "$name" in *"$FILTER"*) ;; *) continue ;; esac
    build="$(make -s -C "$HERE" OUT="$WORK" "$WORK/$name" 2>&1)"
    if [ $? -ne 0 ] || [ ! -x "$WORK/$name" ]; then
        fail=$((fail + 1)); echo "  FAIL  $name: does not build"
        printf '%s\n' "$build" | head -20 | sed 's/^/          /'
        continue
    fi
    if printf '%s\n' "$build" | grep -qi "warning"; then
        fail=$((fail + 1)); echo "  FAIL  $name: the compiler printed warnings"
        printf '%s\n' "$build" | head -20 | sed 's/^/          /'
        continue
    fi
    # shellcheck disable=SC2046   # the arguments are meant to be split into words
    out="$("$WORK/$name" $(args_for "$name") 2>&1)"
    rc=$?
    nok=$(printf '%s\n' "$out" | grep -c "OK")
    nwrong=$(printf '%s\n' "$out" | grep -c "WRONG")
    if [ "$rc" -eq 0 ] && [ "$nok" -gt 0 ] && [ "$nwrong" -eq 0 ]; then
        pass=$((pass + 1)); echo "  ok    $name ($nok checks OK)"
    else
        fail=$((fail + 1)); echo "  FAIL  $name: exit status $rc, $nok OK, $nwrong WRONG"
        printf '%s\n' "$out" | grep -n "WRONG" | head -10 | sed 's/^/          /'
    fi
done
echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
