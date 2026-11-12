#!/usr/bin/env bash
# Build every task and run it, saving output under results/.
# Works with gcc -fopenmp (Linux) or clang + libomp (macOS/Homebrew).
set -u
mkdir -p results

if gcc -fopenmp -x c -o /dev/null - <<<'int main(){return 0;}' 2>/dev/null; then
    CC=gcc; CFLAGS="-O2 -Wall -Wextra -fopenmp"; LD="-fopenmp -lm"
    echo "Using gcc -fopenmp"
else
    LP="$(brew --prefix libomp 2>/dev/null)"
    CC=clang
    CFLAGS="-O2 -Wall -Wextra -Xpreprocessor -fopenmp -I${LP}/include"
    LD="-L${LP}/lib -lomp -lm"
    echo "Using clang + libomp at ${LP}"
fi

for t in task1_vecadd task2_sum task3_maxmin task4_matadd task5_matmul; do
    echo ">>> building $t"
    $CC $CFLAGS "$t.c" -o "$t" $LD || { echo "build failed: $t"; continue; }
    echo ">>> running $t (output -> results/$t.txt)"
    ./"$t" | tee "results/$t.txt"
    echo
done
echo "All done. Timing tables are in results/."
