#!/usr/bin/env bash
# For each question with a solution.patch, (re)create solution/: the complete simulator
# source (../lab6 with the answer applied), buildable with `cd solution && make`.
# Also copies into the question folder every example input its test.sh uses, and adds a
# "## Files" section to its README if it has none.
#   ./make_solutions.sh            all questions
#   ./make_solutions.sh q02 q08    only folders whose name starts with one of these
set -u
cd "$(dirname "$0")"
LAB6=../lab6
EXAMPLES=../minicomputer/examples

for d in q[0-9][0-9][0-9]_*; do
    [ -d "$d" ] || continue
    if [ $# -gt 0 ]; then
        match=0
        for p in "$@"; do case "$d" in "$p"*) match=1 ;; esac; done
        [ "$match" -eq 1 ] || continue
    fi

    # inputs from ../minicomputer/examples that the test uses
    for f in $(grep -oE '[A-Za-z0-9_]+\.(txt|byte)' "$d/test.sh" | sort -u); do
        [ -f "$d/$f" ] || { [ -f "$EXAMPLES/$f" ] && cp "$EXAMPLES/$f" "$d/"; }
    done

    [ -f "$d/solution.patch" ] || continue
    rm -rf "${d:?}/solution"
    mkdir -p "$d/solution"
    cp "$LAB6"/*.cpp "$LAB6"/*.h "$LAB6"/Makefile "$d/solution/"
    if ! (cd "$d/solution" && patch -s -p1 < ../solution.patch); then
        echo "$d: solution.patch does not apply"; continue
    fi
    find "$d/solution" -name '*.orig' -delete

    if ! grep -q '^## Files' "$d/README.md"; then
        inputs=$(cd "$d" && ls *.txt *.byte 2> /dev/null | sed 's/.*/`&`/' | paste -sd, - | sed 's/,/, /g')
        {
            echo
            echo "## Files"
            echo "- \`solution/\`: the complete simulator with this answer applied (lab6 + \`solution.patch\`)."
            echo "  Build and run it directly: \`cd solution && make\`, then \`./sim -b ../<program>.txt ../<data>.byte\`."
            echo "- \`solution.patch\`: only the changed lines, to apply to your own copy: \`patch -p1 < solution.patch\`."
            [ -n "$inputs" ] && echo "- Inputs (programs and data): $inputs."
            echo "- \`test.sh\`: applies the answer to a fresh copy of lab6, builds it and checks the results."
        } >> "$d/README.md"
    fi
    echo "$d: solution/ ($(ls "$d/solution" | wc -l | tr -d ' ') files)"
done
