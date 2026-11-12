#!/usr/bin/env bash
# Write the difference between ../lab6 and your modified simulator folder as a patch:
#   ./make_patch.sh ~/exam/sim > solution.patch
# Apply it to a fresh copy of lab6 with:  cd copy && patch -p1 < solution.patch   (or: git apply -p1 solution.patch)
set -u
[ $# -eq 1 ] && [ -d "$1" ] || { echo "usage: $0 <modified simulator folder>" >&2; exit 1; }
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$(cd "$1" && pwd)"
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/a" "$T/b"
cp "$ROOT"/lab6/*.cpp "$ROOT"/lab6/*.h "$ROOT"/lab6/Makefile "$T/a"/
cp "$SRC"/*.cpp "$SRC"/*.h "$SRC"/Makefile "$T/b"/
cd "$T" && diff -ruN a b
exit 0
