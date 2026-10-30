# OpenMP practice bank — likely lab-test questions

One self-contained C program per question. Each has a serial version, the
parallel version(s), a correctness check against serial (`OK`/`WRONG`), a timing
table over 1/2/4/8 threads (set in code with `omp_set_num_threads()`), and a
`Viva notes` comment at the end with short answers to the likely follow-up questions.
`sample_output/` has the output of every program from an Apple M4 (10 cores),
produced while other benchmarks were running, so treat the timings as rough.
Every program was also built and run under Linux gcc 9.5 and 12.5 with zero
warnings and all checks OK (the only `WRONG` lines are q23's intentional race runs).

```
make                     # builds every q*.c (gcc -O2 -Wall -Wextra -fopenmp)
./q07_count_primes       # default sizes
./q07_count_primes 5000000   # most programs take N as the first argument
```
New question: `cp q00_template.c myq.c`, edit the three marked parts, then
`gcc -O2 -Wall -Wextra -fopenmp myq.c -o myq -lm`.

## Loops and reductions
| File | Question | Constructs |
|---|---|---|
| `q00_template.c` | Template: serial + parallel + verify + timing table | `parallel for reduction` |
| `q01_dot_product.c` | Dot product of two vectors | `reduction(+:)` vs `atomic` vs local + `critical` |
| `q02_pi_integration.c` | π = ∫₀¹ 4/(1+x²) dx | reduction; SPMD partial sums with false sharing vs padding |
| `q03_monte_carlo_pi.c` | Monte Carlo π | per-thread RNG state, `reduction(+:hits)` |
| `q04_mean_variance.c` | Mean, variance, std-dev, min, max | several `reduction` clauses in one loop |
| `q05_matvec.c` | y = A·x | row-parallel vs `reduction(+:y[:n])` |
| `q06_prefix_sum.c` | Prefix sum (scan) | `barrier` + `single` in one region; OpenMP 5 `scan` (gcc ≥ 10) |
| `q07_count_primes.c` | Count primes ≤ N (uneven work) | `static` / `static,1` / `dynamic` / `guided` |
| `q15_string_ops.c` | Vowels, letter frequency, words, pattern count, upper case, palindrome | `reduction(+:f[:26])`, `reduction(&&:)` |

## Arrays and matrices
| File | Question | Constructs |
|---|---|---|
| `q08_count_occurrences.c` | Count, first, last and all positions of a value | `reduction(min:/max:)`, per-thread offsets |
| `q09_vector_ops.c` | SAXPY; norm and normalise in one region | two `omp for` in one region; why not `nowait` |
| `q10_matrix_stats.c` | Row/column sums, trace, Frobenius norm, max with (i,j) | `collapse(2) reduction`, array reduction, local + `critical` |
| `q11_matrix_checks.c` | A == B, symmetric, identity; A − B, k·A | `reduction(&&:)` |
| `q12_array_reverse_rotate.c` | Reverse in place, rotate by k, swap pairs | disjoint writes, no sync |
| `q13_matmul_variants.c` | Matmul ijk / ikj / B-transposed / blocked | cache-friendly loop orders, GFLOP/s |
| `q14_transpose_inplace.c` | In-place and blocked transpose | triangular loop, `schedule(dynamic)` |

## Threads, data sharing and synchronisation
| File | Question | Constructs |
|---|---|---|
| `q20_hello_threads.c` | Hello world; who decides the thread count | runtime functions, `num_threads`, `if` |
| `q21_data_sharing.c` | Value before/inside/after for each clause | `shared` `private` `firstprivate` `lastprivate` `default(none)` `threadprivate` |
| `q22_sync_constructs.c` | Small demo of each sync construct | `barrier` `single` `copyprivate` `master` `nowait` `ordered` `critical(name)` `atomic` |
| `q23_counter_race.c` | Shared counter: race and four fixes | `critical` / `atomic` / `omp_lock_t` / `reduction` |
| `q24_sections.c` | Sum, max, min and sort at the same time | `parallel sections` |
| `q25_locks_histogram.c` | 1024-bin histogram | `critical` / `atomic` / lock per bin / private / `reduction(+:h[:B])` |
| `q26_schedule_runtime.c` | Triangular workload, per-thread imbalance | `schedule(runtime)`, `omp_set_schedule()` |

## Tasks, sorting, stencils, custom reductions
| File | Question | Constructs |
|---|---|---|
| `q27_fibonacci_tasks.c` | Fibonacci with tasks | `task shared(x)`, `taskwait`, cutoff vs none |
| `q28_quicksort_tasks.c` | Quicksort with tasks | `task`, cutoff, verify vs `qsort` |
| `q29_tasks_list_tree.c` | Linked list, binary tree, taskloop, taskgroup | `single` producer, `taskloop grainsize`, `taskgroup` |
| `q30_sorting_parallel.c` | Chunk sort + merge, rank sort, counting sort | pairwise merge rounds, `reduction(+:cnt[:R])` |
| `q31_custom_reduction.c` | Max/min **with index**, second largest | `declare reduction` |
| `q32_convolution_kernels.c` | Blur / Gaussian / sharpen / Sobel; valid and same padding | `collapse(2)`; handout 5×5 example |
| `q33_jacobi_heat.c` | Heat diffusion until convergence | `reduction(max:diff)`, one region vs one per sweep |
| `q34_game_of_life.c` | Game of Life for G generations | double buffering, `collapse(2) reduction(+:pop)` |

## Things the programs show that are easy to get wrong
- At `-O2` gcc keeps a shared counter in a register, which can hide a race; the race demos use `volatile` (`q23`).
- False sharing in `q02` also needs `volatile` to show up, and it is much clearer on x86 than on the Mac.
- libgomp runs a `taskloop` serially if it would create more than 64 × threads tasks (`q29` sizes its grainsize for about 100 tasks).
- gcc older than 12 does not auto-vectorise at `-O2`, so loop-heavy programs are slower on an
  older lab gcc: `q13` took 39 s under gcc 9 (7 s under gcc 15). Pass a smaller N (`./q13_matmul_variants 512`).
- An empty `#pragma omp parallel { }` is deleted by gcc, so a warm-up region needs a statement in it.
- In `q33`, power-of-two grid sizes (e.g. 512) run several times slower because of cache conflicts, and a zero interior produces slow subnormal numbers; see its `INIT` define.
