# CS527 Computer Systems — OpenMP Lab

Serial and OpenMP-parallel implementations of the OpenMP lab tasks (Labs 7, 8
and 9), each with correctness checks and timing, plus a practice bank of likely
lab-test variants.

| Folder | Contents |
|--------|----------|
| `./` (root) | Lab 7: vector add, sum, max/min with index, matrix add, matrix multiply |
| [`session2/`](session2/) | Lab 8: array search, parallel sort, histogram, transpose, 2D convolution |
| [`session3/`](session3/) | Lab 9: merge sort with tasks, static/dynamic/guided scheduling, even/odd count |
| [`practice/`](practice/) | Practice bank: one program per likely test question, plus a template |
| [`problems/`](problems/) | Problem bank: 104 solved problems (statement + tested solution), indexed in [`PROBLEMS.md`](PROBLEMS.md) |
| [`GUIDE.md`](GUIDE.md) | **Start here for a new question:** exam-day steps, the solving method, where to find the closest solution, report template |
| [`MATRIX_RULES.md`](MATRIX_RULES.md) | Matrix indexing, algebra rules, index maps, and which matrix loops are safe to parallelise |
| [`CHEATSHEET.md`](CHEATSHEET.md) | Lab-day commands, OpenMP syntax, answers to every report question |

## Lab day
```
git clone https://github.com/chayonmollick/CS527_CSM10144.git
cd CS527_CSM10144
make && make -C session2 && make -C session3 && make -C practice && make -C problems
```
For a new question, follow [`GUIDE.md`](GUIDE.md), or quickly: `cp practice/q00_template.c myq.c`, edit the three marked
parts, then `gcc -O2 -Wall -Wextra -fopenmp myq.c -o myq -lm`.

## Lab 7 tasks

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
- `REPORT.md` has timings measured on an Apple M4 (10 cores); rerun on the lab
  machine for its own numbers.
