#!/usr/bin/env bash
# Run every question's test.sh (or only those whose folder name contains $1).
#   ./run_all.sh            all 100
#   ./run_all.sh q03        q030..q039
set -u
cd "$(dirname "$0")"
total=0; bad=0
for t in q[0-9][0-9][0-9]_*/test.sh; do
    d="${t%/test.sh}"
    case "$d" in *"${1:-}"*) ;; *) continue ;; esac
    total=$((total + 1))
    if out="$(bash "$t" 2>&1)"; then
        printf '  ok    %-40s %s\n' "$d" "$(echo "$out" | tail -1)"
    else
        bad=$((bad + 1)); printf '  FAIL  %-40s %s\n' "$d" "$(echo "$out" | tail -1)"
        echo "$out" | grep -E "FAIL|error|does not apply" | head -5 | sed 's/^/          /'
    fi
done
echo
echo "$((total - bad)) of $total questions pass"
[ "$bad" -eq 0 ]
