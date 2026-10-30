# CS527 Computer Systems — OpenMP Lab 9 (Session 3)

Serial and OpenMP-parallel implementations of three kernels. Each program checks
every parallel result against the serial one and times 1/2/4/8 threads (set
in code with `omp_set_num_threads()`). Every timing is the best of 3 runs.

| File | Task | OpenMP focus |
|------|------|--------------|
| `task1_mergesort.c` | Merge sort | `task` + `taskwait`, cutoff threshold, cutoff sweep |
| `task2_schedule.c` | Uneven per-element work | `schedule(static)` / `(dynamic, chunk)` / `(guided)`, chunk sweep via `schedule(runtime)` |
| `task3_evenodd.c` | Even/odd count | `reduction(+:even, odd)` vs `atomic` vs per-thread counters |

## Build & run
```
make                 # gcc -O2 -Wall -Wextra -fopenmp
./task1_mergesort            # N = 1e5, 1e6, 1e7, then a cutoff sweep
./task1_mergesort 5000000 4096   # one N, chosen cutoff
./task2_schedule             # N = 1e5, 1e6, 1e7 for 3 workloads, then a chunk sweep
./task2_schedule 1000000 500 10  # one N, avg work per element, dynamic chunk
./task3_evenodd              # N = 1e5 .. 1e8
./run_benchmarks.sh          # all of the above, saved to results/
```
On macOS without GNU gcc, the Makefile falls back to clang + Homebrew libomp.

Written answers and timing tables are in `REPORT.md`.
