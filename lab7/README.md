# Lab 7 — Getting started with OpenMP (arrays and matrices)

Serial and OpenMP-parallel versions of five kernels. Each program checks the
parallel result against the serial one and prints a timing table for 1, 2, 4 and
8 threads (set in code with `omp_set_num_threads()`). Written answers and measured
timings are in [`REPORT.md`](REPORT.md).

| File | Kernel | OpenMP constructs exercised |
|------|--------|-----------------------------|
| `task1_vecadd.c` | Vector addition `c = a + b` | `parallel for` |
| `task2_sum.c`    | Sum of elements | naive race (plain and `volatile`) vs `reduction(+:)` vs `critical` |
| `task3_maxmin.c` | Max/Min value **and** index | `reduction(max:/min:)`, `critical`, manual per-thread reduction |
| `task4_matadd.c` | Matrix addition `C = A + B` | outer `parallel for` vs `collapse(2)` |
| `task5_matmul.c` | Matrix multiply `C = A·B` | parallel `i` vs `collapse(2)`; bonus `ikj` order |

## Build and run
```bash
make                  # gcc -O2 -Wall -Wextra -fopenmp (on macOS it finds Homebrew gcc or clang + libomp)
./task1_vecadd        # each program prints its own correctness check and timing table
./run_benchmarks.sh   # builds everything and saves the output in results/
```
- Thread counts are swept inside the programs (1, 2, 4, 8), so one run gives the
  whole table. `export OMP_NUM_THREADS=n` still caps the machine.
- `task1` tries N up to 1e8 (about 3.2 GB for four arrays) and skips a size if the
  allocation fails.
- `REPORT.md` has timings measured on an Apple M4 (10 cores); rerun on the lab
  machine for its numbers.
