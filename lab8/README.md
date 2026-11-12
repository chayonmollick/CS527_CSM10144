# CS527 Computer Systems — OpenMP Lab 8 (Session 2)

Serial and OpenMP-parallel implementations of five kernels, each with a
correctness check against the serial result and a timing sweep over 1/2/4/8
threads (`omp_set_num_threads()` in-code).

| File | Task | OpenMP focus |
|------|------|--------------|
| `task1_search.c` | Array search (exists? + index) | `reduction(min:)` for first index |
| `task2_sort.c` | Parallel sort (odd-even transposition) | phase barrier as synchronisation |
| `task3_histogram.c` | Histogram of 0..9 | race demo, per-thread bins, `reduction(+:h[:10])` |
| `task4_transpose.c` | Matrix transpose | outer vs `collapse(2)` |
| `task5_convolution.c` | 3x3 averaging convolution (valid) | `collapse(2)` over output pixels |

## Build & run
```
make                 # gcc -O2 -Wall -Wextra -fopenmp
./run_benchmarks.sh  # builds all, saves output to results/
```
On macOS without GNU gcc, `run_benchmarks.sh` falls back to clang + Homebrew libomp.

Written answers and timing tables are in `REPORT.md`.
