# OpenMP Lab Test — Cheat Sheet

Everything needed offline: lab-day commands, where each solution lives, OpenMP
syntax, which construct fits which problem, and short answers to every report
question from Labs 7, 8 and 9.

---

## 0. First five minutes in the lab

```bash
git clone https://github.com/chayonmollick/CS527_CSM10144.git   # public repo, no login needed
cd CS527_CSM10144

nproc                                   # logical cores -> decides the thread counts worth testing
lscpu | grep -E 'Model name|^CPU\(s\)|Thread|Core|Socket'
gcc --version | head -1
echo | gcc -fopenmp -dM -E - | grep _OPENMP   # 201511 = OpenMP 4.5 (gcc reports this even for gcc 15)

make                    # Lab 7  (task1..task5 in the root)
make -C session2        # Lab 8
make -C session3        # Lab 9
make -C practice        # practice bank q01..q34
```

- Compile one file: `gcc -O2 -Wall -Wextra -fopenmp file.c -o file -lm`
- Threads: every program here sets them **in code** with `omp_set_num_threads()` over 1/2/4/8.
  The other way is `export OMP_NUM_THREADS=4` before running. State which one you used.
- Save output for the report: `./task1_mergesort | tee out_task1.txt`
- Most programs take `N` as the first argument: `./task3_evenodd 50000000`.
- New question? `cp practice/q00_template.c myq.c`, edit the 3 marked parts, build, run.
- If the lab PC has fewer than 8 cores, 8 threads still runs (oversubscribed); say so when explaining the numbers.
- `-fopenmp` missing ⇒ pragmas are silently ignored (serial code) and `omp_get_wtime` fails to link.

---

## 1. Where every solution is

| Lab | Task | File |
|---|---|---|
| 7 | Vector addition | `task1_vecadd.c` |
| 7 | Sum: naive race / `reduction` / `critical` | `task2_sum.c` |
| 7 | Max/min **with index**: reduction, critical, manual per-thread | `task3_maxmin.c` |
| 7 | Matrix addition: outer vs `collapse(2)` | `task4_matadd.c` |
| 7 | Matrix multiply: `i` vs `collapse(2)`, bonus `ikj` | `task5_matmul.c` |
| 8 | Array search (exists + first index) | `session2/task1_search.c` |
| 8 | Parallel sort (odd-even transposition) | `session2/task2_sort.c` |
| 8 | Histogram 0..9 (race demo, private bins, array reduction) | `session2/task3_histogram.c` |
| 8 | Matrix transpose | `session2/task4_transpose.c` |
| 8 | 3×3 convolution (handout 5×5 example) | `session2/task5_convolution.c` |
| 9 | Merge sort with `task`/`taskwait` + cutoff | `session3/task1_mergesort.c` |
| 9 | `static` / `dynamic` / `guided` scheduling | `session3/task2_schedule.c` |
| 9 | Even/odd count: `reduction(+:even,odd)` vs `atomic` | `session3/task3_evenodd.c` |

Practice bank (likely test variants), all in `practice/` — see `practice/README.md`:

| File | Question |
|---|---|
| `q00_template.c` | Blank serial + parallel + verify + timing template |
| `q01_dot_product.c` | Dot product: reduction vs atomic vs local+critical |
| `q02_pi_integration.c` | π by numerical integration; false sharing vs padding |
| `q03_monte_carlo_pi.c` | Monte Carlo π with per-thread RNG |
| `q04_mean_variance.c` | Mean, variance, std-dev, min, max (multiple reductions) |
| `q05_matvec.c` | Matrix–vector product (row-parallel vs array reduction) |
| `q06_prefix_sum.c` | Prefix sum / scan (two-pass blocked) |
| `q07_count_primes.c` | Count primes (uneven work, schedules) |
| `q08_count_occurrences.c` | Count / first / last / all positions of a value |
| `q09_vector_ops.c` | SAXPY, norm and normalise in one parallel region |
| `q10_matrix_stats.c` | Row/column sums, trace, Frobenius norm, max with (i,j) |
| `q11_matrix_checks.c` | A==B, symmetric, identity via `reduction(&&:)` |
| `q12_array_reverse_rotate.c` | Reverse in place, rotate, swap pairs |
| `q13_matmul_variants.c` | Matmul ijk / ikj / transposed-B / blocked |
| `q14_transpose_inplace.c` | In-place and blocked transpose |
| `q15_string_ops.c` | Text: vowels, letter frequency, words, pattern count, upper case, palindrome |
| `q20_hello_threads.c` | Thread ids, counts, `num_threads`, `if` clause |
| `q21_data_sharing.c` | shared / private / firstprivate / lastprivate / threadprivate |
| `q22_sync_constructs.c` | barrier, single, master, nowait, ordered, critical, atomic |
| `q23_counter_race.c` | Shared counter: race / critical / atomic / lock / reduction |
| `q24_sections.c` | `parallel sections` |
| `q25_locks_histogram.c` | Big histogram: critical / atomic / per-bin locks / private / reduction |
| `q26_schedule_runtime.c` | Triangular workload, `schedule(runtime)` |
| `q27_fibonacci_tasks.c` | Fibonacci with tasks (cutoff vs none) |
| `q28_quicksort_tasks.c` | Quicksort with tasks |
| `q29_tasks_list_tree.c` | Linked list / tree with tasks, `taskloop`, `taskgroup` |
| `q30_sorting_parallel.c` | Chunk-sort + merge, rank sort, counting sort |
| `q31_custom_reduction.c` | `declare reduction` for max/min **with index**, second largest |
| `q32_convolution_kernels.c` | Blur / sharpen / Sobel, valid and same padding |
| `q33_jacobi_heat.c` | Jacobi / heat diffusion with convergence check |
| `q34_game_of_life.c` | Game of Life |

---

## 2. Syntax quick reference

```c
#include <omp.h>

#pragma omp parallel                       // team of threads runs the block
#pragma omp parallel num_threads(4) if(n > 10000)

#pragma omp parallel for                   // split loop iterations (implicit barrier at end)
#pragma omp parallel for collapse(2)       // fuse 2 perfectly nested loops into one space
#pragma omp parallel for schedule(static)          // equal contiguous blocks, no overhead
#pragma omp parallel for schedule(static, 4)       // round-robin chunks of 4
#pragma omp parallel for schedule(dynamic, 100)    // grab 100 at a time when free
#pragma omp parallel for schedule(guided)          // big chunks first, shrinking
#pragma omp parallel for schedule(runtime)         // from OMP_SCHEDULE / omp_set_schedule()

#pragma omp parallel for reduction(+:sum)
#pragma omp parallel for reduction(+:even, odd)               // several vars, same op
#pragma omp parallel for reduction(+:s) reduction(max:mx) reduction(min:mn)
#pragma omp parallel for reduction(+:hist[:10])               // array section (OpenMP 4.5)
//   operators: + - * & | ^ && || max min     (max/min need OpenMP 3.1+)

// data sharing
shared(a)  private(x)  firstprivate(x)  lastprivate(x)  default(none)
#pragma omp threadprivate(counter)        // + copyin(counter) on the parallel

// synchronisation
#pragma omp critical            { ... }   // one thread at a time (any code)
#pragma omp critical(name)      { ... }   // named: different names don't block each other
#pragma omp atomic                        // single memory update: x++, x += e, x = x op e
#pragma omp atomic capture                // v = x++;  (read + update atomically)
#pragma omp barrier
#pragma omp single              { ... }   // one thread, others wait at the end (unless nowait)
#pragma omp master              { ... }   // thread 0 only, NO barrier
#pragma omp for nowait                    // drop the implicit barrier
#pragma omp for ordered  +  #pragma omp ordered { ... }   // run that block in loop order

// sections
#pragma omp parallel sections
{
    #pragma omp section
    { work_a(); }
    #pragma omp section
    { work_b(); }
}

// tasks
#pragma omp parallel
{
    #pragma omp single
    solve(0, n - 1);                     // one thread creates tasks, all threads run them
}
#pragma omp task shared(x) firstprivate(lo, hi)
#pragma omp taskwait                      // wait for this task's direct children
#pragma omp taskgroup { ... }             // wait for all descendants
#pragma omp taskloop grainsize(1000)

// locks
omp_lock_t l; omp_init_lock(&l); omp_set_lock(&l); /*...*/ omp_unset_lock(&l); omp_destroy_lock(&l);

// custom reduction (max with index)
typedef struct { double val; long idx; } pair;
#pragma omp declare reduction(maxloc : pair : omp_out = (omp_in.val > omp_out.val || \
        (omp_in.val == omp_out.val && omp_in.idx < omp_out.idx)) ? omp_in : omp_out) \
        initializer(omp_priv = (pair){-1e300, -1})
```

Runtime functions: `omp_get_thread_num()`, `omp_get_num_threads()` (1 outside a region),
`omp_get_max_threads()`, `omp_get_num_procs()`, `omp_set_num_threads(n)`,
`omp_get_wtime()`, `omp_in_parallel()`, `omp_set_schedule(omp_sched_dynamic, 100)`.

Environment: `OMP_NUM_THREADS=4`, `OMP_SCHEDULE="dynamic,100"`, `OMP_PROC_BIND=true`,
`OMP_CANCELLATION=true` (needed for `#pragma omp cancel for`).

Manual block bounds (when the question says "use `omp_get_thread_num()`"):
```c
#pragma omp parallel
{
    int t = omp_get_thread_num(), p = omp_get_num_threads();
    long lo = n * t / p, hi = n * (t + 1) / p;     // this thread's [lo, hi)
    for (long i = lo; i < hi; i++) { ... }
}
```

2D matrices: this repo uses flattened row-major `A[(long)i*n + j]`. If the question
wants `double **A` with `A[i][j]` (as in the Lab 7 sample), allocate it contiguously:
```c
double **alloc_matrix(int n) {
    double **A = malloc(n * sizeof(double *));
    double *data = calloc((size_t)n * n, sizeof(double));     // one block, zeroed
    for (int i = 0; i < n; i++) A[i] = data + (size_t)i * n;
    return A;                                   // free(A[0]); free(A);
}
```

---

## 3. Which construct for which problem

| Problem shape | Use | Example file |
|---|---|---|
| Independent iterations, each writes its own element | `parallel for` | vecadd, matadd, transpose, convolution |
| Combine everything into one value (sum, count, max) | `reduction(op:var)` | sum, dot product, even/odd |
| Value **and** where it is (argmax) | per-thread local best + `critical` merge, or `declare reduction` | `task3_maxmin.c`, `q31` |
| Many counters (histogram) | `reduction(+:h[:B])` or private arrays merged once | `session2/task3_histogram.c`, `q25` |
| Few outer iterations (N=8) | `collapse(2)` | `task4_matadd.c` |
| Iterations of very different cost | `schedule(dynamic, c)` or `guided` | `session3/task2_schedule.c`, `q07`, `q26` |
| Recursion / divide-and-conquer / linked list | `task` + `taskwait` with a cutoff | merge sort, quicksort, fib |
| A few different jobs at once | `sections` (or tasks) | `q24` |
| Stages that depend on each other (sort phases, prefix sum) | barrier between stages (implicit at end of `omp for`) | odd-even sort, `q06` |
| Shared counter, rare updates | `atomic` (single op) / `critical` (block) | `q23` |

---

## 4. Answers to the report questions

### Lab 7 (Arrays and matrices)

**Vector add: when does parallel pay off, why not for small N?** Opening a parallel
region (waking threads, splitting the loop, the end barrier) costs microseconds.
For N ≈ 10^5 the whole loop takes about as long, so there's no gain; from about
10^6 upward the loop dominates.
**Does speedup grow linearly?** No. `c = a + b` does 1 add per 24 bytes moved, so it is
memory-bandwidth bound; once 2–4 threads saturate the memory bus, more threads
don't help.

**Naive sum is wrong because** `sum += a[i]` is load, add, store on a **shared** `sum`.
Two threads read the same old value, both store, one update is lost (data race).
The result is wrong and different every run.
**`critical` fixes correctness but** serialises every iteration behind a lock, which
is slower than serial. `reduction` gives each thread a private partial sum and
combines once at the end, so it is both correct and fast.

**Why not `reduction(max: best_idx)`?** We maximise the value, not the index; the
index has to travel with the winning value. A scalar max on the index gives
the largest index, which is meaningless. It needs a pair reduction.
**Critical vs manual per-thread (val, idx)?** Manual is far faster: no lock in the
hot loop, only one combine per thread. Critical serialises every iteration.

**Matrix add, small N = 8:** outer-only exposes 8 iterations (at most 8 busy threads,
poor balance); `collapse(2)` exposes 64. Collapse helps.
**Large N = 4096:** outer loop already has 4096 ≫ threads iterations, so collapse makes
no measurable difference (and it is memory-bound anyway).

**Matmul, why is parallel `i` safe?** Each thread owns whole rows of C; no two threads
write the same `C[i][j]`; the k-sum goes into a thread-local `sum`.
**Parallel `k` directly:** all threads do `C[i][j] += …` on the same element, a race
with lost updates. It would need `reduction`. That's also why `k` must not be collapsed.
**When does collapse(2) matter?** When N is small relative to the thread count (N ≲ p,
or N not a multiple of p). For N = 1024 on 8 threads it doesn't matter.
**Local `sum` instead of `C[i][j] +=`:** stays in a register, so there's one store per
element instead of N loads and stores.
**Bonus ikj:** inner loop walks B and C row-wise (stride 1), so the serial version is much faster and scales better.

### Lab 8 (Search, sort, histogram, transpose, convolution)

**Search:** `reduction(min:found)` with sentinel n gives the *first* index
deterministically. Early exit needs `#pragma omp cancel for` + `OMP_CANCELLATION=true`
(you can't `break` out of an omp for). It is memory-bound, so speedup is small.
**Sort, synchronisation needed:** odd-even transposition. Within one phase the pairs
are disjoint, so there's no race; between phases a **barrier** is needed (the implicit one at
the end of each `omp for`). Bubble sort itself can't be parallelised: each pass
depends on the previous swap.
**Histogram race:** `h[a[i]]++` on shared bins loses updates. Fix: private bins + one
merge, `reduction(+:h[:10])`, or `atomic` (correct but slow).
**Transpose:** each `B[j][i]` is written exactly once, so there's no race. Large N is
limited by strided writes (cache misses); blocking/tiling helps.
**Convolution:** each output pixel reads a read-only 3×3 window and writes its own
cell, so it's race-free; use `collapse(2)` over output pixels. Valid output is (n−2)×(n−2);
the handout's first value is 630/9 = 70.

### Lab 9 (Tasks, scheduling, multiple reductions) — details in `session3/REPORT.md`

**Tasks vs parallel for:** `for` splits a loop whose iteration count is known at
entry, statically or dynamically, and ends with a barrier. Tasks are units of
work created on the fly (recursively, unknown count) and executed by any idle
thread. That makes them right for recursion, trees, lists and irregular work.
**Why taskwait before merge?** The child tasks may still be running; merge reads both
halves, so it must wait until both are fully sorted. Otherwise it reads half-sorted data and the output is wrong.
**Cutoff:** too small means a huge number of tiny tasks, and creation/scheduling overhead dominates
(slower than serial). Too large means too few tasks to keep threads busy (cutoff ≥ N
is fully serial). Aim for tens of tasks per thread.
**Why slower than serial for small arrays?** Thread-team start-up and task overhead are
comparable to the whole sort. The top-level merge is O(n) and serial (Amdahl).
**Task overhead and scalability:** each task costs an allocation, a queue push/pop and a
`taskwait`. The merges near the root are serial, so span = O(n) vs work O(n log n),
and speedup is capped near log₂ n regardless of thread count.

**static / dynamic / guided:** static assigns iterations up front (contiguous blocks,
or round-robin chunks of c) with zero runtime cost. dynamic: each idle thread grabs the
next chunk (default 1) from a shared counter. guided: like dynamic, but chunks start
at about remaining/threads and shrink, so there are fewer grabs.
**Uniform work: static is best** (no overhead, good locality). **Uneven work: dynamic or guided.**
A *periodic* uneven pattern (handout's `i % 1000`) is still balanced by static,
because each big block contains many whole periods. A ramp is not: static's last
block holds the most work (speedup cap = p²/(2p−1) ≈ 4.27 on 8 threads).
**Dynamic overhead:** each chunk grab is an atomic update of a shared counter (cache-line
ping-pong). Tiny chunks also put neighbouring writes on different threads (false sharing) and lose locality.
**Chunk size:** small means better balance but more overhead; large means less overhead but imbalance
returns (chunk = N/p behaves like static). Pick many more chunks than threads.

**Multiple reduction variables:** `reduction(+:even, odd)`, or separate clauses for
different operators. Each gets a private copy initialised to the identity (0 for +).
**Reduction vs atomic:** reduction updates a private register and combines once per
thread. Atomic does an RMW on a shared cache line every iteration, so the line bounces
between cores and gets *slower* as threads are added.
**even + odd = N:** every element is exactly one of even/odd and every iteration runs
exactly once, so the counts partition N. A mismatch would reveal lost updates.
Note `-3 % 2 == -1` in C, so test `== 0` / `!= 0`, never `== 1`.
**More threads always faster?** No: small N (overhead), memory-bound loops (bandwidth),
atomics (contention), more threads than cores (oversubscription).

---

## 5. General viva points

- **Speedup** S = T_serial / T_parallel; **efficiency** E = S / p.
- **Amdahl:** S ≤ 1 / (f + (1−f)/p), where f is the serial fraction. With f = 5%, S ≤ 20 forever.
- **Why speedup is sub-linear:** memory bandwidth, fork/join + barrier overhead, load
  imbalance, synchronisation (critical/atomic), false sharing, serial parts (Amdahl),
  hyperthreads ≠ cores, other processes.
- **Parallel with 1 thread slower than serial:** region overhead, and gcc optimises the
  OpenMP-outlined loop less (it can't inline into it, propagate constants or prove two
  arrays don't overlap, so it may not vectorise). `practice/q15_string_ops.c` shows it.
- **Superlinear speedup** (S > p) happens when each thread's slice fits in its cache, or when
  the parallel build generates different (better) code than the serial one.
- **Data race:** two threads access the same location, at least one writes, no ordering.
- **False sharing:** threads write *different* variables on the *same* 64-byte cache line,
  so the line ping-pongs. Fix: pad, or use local variables and write once.
- **critical vs atomic:** atomic is one hardware RMW on one scalar (cheap). critical is a
  lock around any block (expensive). Both serialise.
- **Implicit barriers:** at the end of `parallel`, `for`, `single`, `sections`. None at the end of `master`.
- **Default sharing:** variables declared outside the region are shared; declared inside, private;
  the `omp for` loop index is private.
- **First touch:** pages are placed in memory when first written, which is why the first
  timed run is slower. Time the best of several runs.

## 6. Common compile/run errors

| Symptom | Fix |
|---|---|
| `undefined reference to omp_get_wtime` | add `-fopenmp` to the link command too |
| pragmas do nothing, 1 thread | missing `-fopenmp` |
| `undefined reference to sqrt` | add `-lm` at the end |
| `M_PI undeclared` | `#define PI 3.14159265358979323846` |
| `break statement used with OpenMP for loop` | use a flag, or `cancel for` |
| `collapse` error | loops must be perfectly nested, with nothing between the two `for`s |
| `reduction` on an array: error | needs `h[:n]` section syntax (OpenMP 4.5, gcc ≥ 6) |
| segfault with a big local array | allocate with `malloc`, not on the stack |
| `%d` warning for long | use `%ld` |
| different result every run | race on a shared variable: make it private/reduction |
| race demo prints the *correct* answer | at -O2 gcc keeps the shared counter in a register (one store per thread); declare it `volatile` or build with `-O0` to show the race (`practice/q23_counter_race.c`) |
| warm-up `#pragma omp parallel { }` has no effect | gcc deletes empty regions; put a statement in it, e.g. `{ volatile int id = omp_get_thread_num(); (void)id; }` |
