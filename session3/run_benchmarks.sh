#!/usr/bin/env bash
# Build every task with the Makefile (auto-detects the OpenMP toolchain) and run
# it, saving output under results/.
set -u
cd "$(dirname "$0")"
make || exit 1
mkdir -p results
for t in task1_mergesort task2_schedule task3_evenodd; do
    echo ">>> running $t"; ./"$t" | tee "results/$t.txt"; echo
done
echo "Done. Timing tables in results/."
