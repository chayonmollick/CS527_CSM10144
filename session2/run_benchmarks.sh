#!/usr/bin/env bash
# Build and run every task, saving output under results/.
set -u
mkdir -p results
if gcc -fopenmp -x c -o /dev/null - <<<'int main(){return 0;}' 2>/dev/null; then
    CC=gcc; CFLAGS="-O2 -Wall -Wextra -fopenmp"; LD="-fopenmp -lm"; echo "Using gcc -fopenmp"
else
    LP="$(brew --prefix libomp 2>/dev/null)"; CC=clang
    CFLAGS="-O2 -Wall -Wextra -Xpreprocessor -fopenmp -I${LP}/include"; LD="-L${LP}/lib -lomp -lm"
    echo "Using clang + libomp at ${LP}"
fi
for t in task1_search task2_sort task3_histogram task4_transpose task5_convolution; do
    echo ">>> building $t"; $CC $CFLAGS "$t.c" -o "$t" $LD || { echo "build failed: $t"; continue; }
    echo ">>> running $t"; ./"$t" | tee "results/$t.txt"; echo
done
echo "Done. Timing tables in results/."
