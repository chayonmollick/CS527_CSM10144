# CS527 Computer Systems — OpenMP Lab

Serial and OpenMP-parallel implementations of five array/matrix kernels, with
correctness checks and timing.

## Tasks
| File | Kernel | OpenMP constructs exercised |
|------|--------|-----------------------------|
| `task1_vecadd.c` | Vector addition `c = a + b` | `parallel for` |
| `task2_sum.c`    | Sum of elements | naive race vs `reduction(+:)` vs `critical` |
| `task3_maxmin.c` | Max/Min value **and** index | `reduction(max:/min:)`, `critical`, manual per-thread reduction |
| `task4_matadd.c` | Matrix addition `C = A + B` | outer `parallel for` vs `collapse(2)` |
| `task5_matmul.c` | Matrix multiply `C = A·B` | parallel `i` vs `collapse(2)`; bonus `ikj` order |

## Build
```
make            # builds all five with: gcc -O2 -Wall -Wextra -fopenmp
```
(macOS without a GNU gcc: use `./run_benchmarks.sh`, which falls back to
`clang` + Homebrew `libomp`.)

## Run
```
./task1_vecadd        # each program prints its own timing table
./run_benchmarks.sh   # builds everything and saves output to results/
```

## Threads
Thread counts are swept **inside the programs** with `omp_set_num_threads()`
(1, 2, 4, 8), so a single run produces the whole table. You can still cap the
machine with `export OMP_NUM_THREADS=n` before running.

## Notes
- Every parallel kernel is checked element-wise against its serial version;
  the tables print `yes`/`verified` on success.
- `task1` attempts `N` up to 1e8 (≈3.2 GB across four arrays) and skips a size
  gracefully if allocation fails.
- Timing numbers in `REPORT.md` are to be filled from a run on the lab machine.
