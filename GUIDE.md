# How to solve a new OpenMP question in the lab test

Use this when the exam gives a problem that is not exactly one of the labs. It
explains how to go from the problem statement to a verified, timed, explained
solution, and how to find the closest solved problem in this repo.

Companion files:
- `PROBLEMS.md`: index of 104 solved problems (statement + solution file).
- `MATRIX_RULES.md`: matrix indexing, algebra rules and which matrix loops are safe to parallelise.
- `CHEATSHEET.md`: OpenMP syntax and answers to the lab report questions.

---

## 0. Exam day, based on how the last CS527 lab test ran (Lab 6 Test, 12 Sep)

The last test went like this: a system is assigned to you at 2 PM, you get about
20 minutes to set up and test your code, the problem statement comes out at about
2:25 PM, and then the network goes off. Only the IDE, the problem PDF and
cppreference may be open. At the end you upload a `tar` archive to Classroom.
That test (Lab 6) asked you to *extend your own lab code* with a new feature, so
expect a new problem built on the OpenMP lab tasks.

**In the 20-minute setup window:**
```bash
git clone https://github.com/chayonmollick/CS527_CSM10144.git
cd CS527_CSM10144
nproc; gcc --version | head -1            # note cores + gcc version for the report
make -C problems -s && ./problems/p000_example 1000000    # proves gcc + OpenMP work
tar --version | head -1                   # the submission needs tar
mkdir -p ~/exam && cp problems/common.h practice/q00_template.c ~/exam/
```
- Download the problem statement as a **PDF** and keep a local copy (as instructed last time).
- Open `GUIDE.md`, `PROBLEMS.md` and `CHEATSHEET.md` in the IDE. They are files in your
  own repo, so no browser tab is needed.

**Submission** (last time: archive the folder, upload to Classroom in the final minutes):
```bash
cd ~ && tar -czvf <entry_no>_labtest.tgz exam/      # e.g. name it like your Lab 6 file 2026CSM1044_lab6.zip
tar -tzvf <entry_no>_labtest.tgz                    # check what's inside before uploading
```
Put the source files, a `README.md` (machine, how to build/run, timing tables,
explanations) and the program outputs (`./prog | tee out.txt`) in the archive.

**Time plan for about 3.5 hours:**

| Time | What |
|---|---|
| 0:00–0:15 | Read the problem twice; list required versions, sizes, thread counts, questions |
| 0:15–0:30 | Find the closest solved problem (section 2), copy it into `~/exam` |
| 0:30–1:30 | Serial version + checker, then the parallel version(s); everything prints OK |
| 1:30–2:15 | Timing runs for all required sizes/threads, saved with `tee` |
| 2:15–3:00 | README: tables, speedup/efficiency, answers to the questions |
| 3:00–end | Re-run everything once from a clean build, then `tar` and submit |

Submit *something that compiles and is correct* early, then improve it. A correct
serial + simple parallel version beats a clever broken one.

---

## 1. The method (7 steps)

### Step 1 — Read and list the deliverables
Write down: the input (array/matrix/graph/text, its size N), the output, every
**version** asked for (serial, parallel, variant A vs B, e.g. critical vs atomic), the
**sizes** and **thread counts** for timing, and every **question** to answer.

### Step 2 — Classify the computation (pattern) and find a model solution
Use the table in section 2. Most exam problems are one of about 15 patterns.

### Step 3 — Write the serial version first, plus a checker
The serial version is the reference. Keep it simple and obviously correct. Write the
check now: `same_ints` / `close_doubles` / `close1` from `problems/common.h`,
or problem-specific checks (sorted? `even + odd == N`? residual ‖Ax−b‖ small?).

### Step 4 — Dependency analysis: can this loop run in parallel?
For the loop you want to parallelise, ask for every pair of iterations i ≠ j:
**does iteration j write a memory location that iteration i reads or writes?**

| What you find | Name | What to do | Example |
|---|---|---|---|
| Each iteration writes only its own `out[i]`, reads only inputs | independent (map) | `parallel for` | vector add, convolution, per-pixel ops |
| All iterations update one scalar (`sum +=`, `count++`, `max =`) | reduction | `reduction(op:var)` | sum, dot product, count |
| Iterations update elements of a shared array at data-dependent places (`h[a[i]]++`) | scattered update | `reduction(+:h[:B])`, private arrays + merge, or `atomic` | histogram, k-means sums |
| Need the value *and* where it is (argmax, first index) | pair reduction | per-thread best + `critical` merge, or `declare reduction` | max with index |
| `a[i]` uses `a[i-1]` computed in this loop | true (flow) dependency | reformulate: scan / closed form / independent recomputation; or keep sequential | prefix sum, recurrence, Fibonacci |
| `a[i]` uses `a[i+1]` (old value) | anti-dependency | read from a copy of the old array | shift left |
| In-place grid update reads neighbours updated in the same sweep | stencil dependency | Jacobi (double buffer), red-black, or wavefront (anti-diagonals) | Gauss–Seidel, LCS, `x[i][j]=x[i-1][j]+x[i][j-1]` |
| Outer loop step k needs results of step k−1 | sequential outer loop | keep k serial, parallelise the inner loops of each step | LU, Gaussian elimination, Floyd–Warshall, time steps |
| Recursive divide and conquer, linked structures | irregular | `task` + `taskwait` with a cutoff | merge/quick sort, tree sum, N-Queens |
| Iterations have very different costs | load imbalance | `schedule(dynamic, c)` / `guided` | primes, Mandelbrot, triangular loops |
| A few different jobs | functional parallelism | `sections` or tasks | compute min, max, sort at once |

Shortcuts:
- A variable assigned before it is read **in the same iteration** (a temporary) is fine:
  declare it inside the loop body, or `private`.
- A loop index or counter that is derived from i (`k = k + 2`) is an induction variable:
  rewrite it as `k = 2*i + k0`.
- `break`/`return` inside the loop is not allowed in `omp for`. Use a flag, or `cancel for`.

### Step 5 — Parallelise, then check for races
Race checklist (go through it every time):
1. Every variable written inside the parallel loop is either **private** (declared inside the body,
   loop index, `private`), a **reduction** variable, or an array element **owned by one iteration**.
2. Nothing is written by one iteration and read by another without a barrier in between.
3. Shared counters/accumulators are reductions or atomics, never a plain `x++`.
4. Results that depend on order (first index, ties) have an explicit tie-break.
5. Random numbers: no `rand()` in parallel code (shared state). Use a per-thread or counter-based generator.
6. Work between two dependent loops inside one `parallel` region has a barrier
   (implicit at the end of `omp for`/`single`; **no** `nowait` there).

Choose the schedule: uniform cost → default `static`; uneven → `dynamic, c` (c ≈ 16–1000)
or `guided`; triangular loops → `dynamic` / `static,1` / `guided`.

### Step 6 — Verify, then measure
- Run with **1 thread** first: it must give the serial answer exactly (or within tolerance).
- Then 2/4/8 threads; the check must stay OK on every run (run more than once: races are random).
- Time with `omp_get_wtime()`, the **best of 3** runs (`BEST_TIME`), after a warm-up region.
  Don't time allocation, initialisation or printing.
- Report speedup S = T_serial / T_parallel and efficiency E = S / p.
- Sizes: small N shows overhead and large N shows scaling. Include both if allowed.

### Step 7 — Explain the numbers (2–3 lines per table)

| Observation | Explanation to write |
|---|---|
| Parallel slower for small N | fork/join + scheduling overhead (µs) is comparable to the work |
| Speedup flattens at 2–4 threads | memory-bandwidth bound (few flops per byte moved) |
| Speedup close to p | compute-bound and independent iterations (e.g. matmul, Mandelbrot with dynamic) |
| Efficiency drops at 8 threads | more threads than (performance) cores, hyper-threads, shared caches/bandwidth |
| critical/atomic version slow, gets slower with threads | serialisation + cache-line ping-pong (contention) |
| static bad, dynamic/guided good | uneven work per iteration (load imbalance) |
| dynamic with chunk 1 slow | one synchronised grab per iteration + false sharing of output |
| 1-thread parallel slower than serial | region overhead; outlined loop optimised less (no inlining/vectorisation) |
| Superlinear speedup | each thread's slice fits in cache |
| Tasks slower than serial | too many tiny tasks: raise the cutoff |
| Amdahl | S ≤ 1 / (f + (1−f)/p): the serial part f caps the speedup |

---

## 2. Find the closest solved problem

Search the repo from the terminal:
```bash
grep -n "Problem:" problems/*.c | grep -i "matrix"        # statements mentioning a word
grep -il "wavefront\|anti-diagonal" problems/*.c practice/*.c
grep -l "declare reduction" problems/*.c practice/*.c      # files using a construct
```

| Pattern | Look at |
|---|---|
| Map: independent element-wise work | `task1_vecadd.c`, `p021`, `p022`, `p055`, `p080` |
| Sum/count/norm reductions | `task2_sum.c`, `p000`, `p023`, `p025`, `session3/task3_evenodd.c` |
| Several reductions / two phases in one region | `p024`, `p004`, `practice/q04`, `practice/q09` |
| Max/min **with index**, top-k, smallest nonzero | `task3_maxmin.c`, `p011`, `p032`, `p033`, `practice/q31` |
| Custom (user-defined) reduction, mod arithmetic, structs | `p026`, `p101`, `practice/q31` |
| Search: exists / first / last / all positions / early exit | `session2/task1_search.c`, `p005`, `p036`, `practice/q08`, `p054` |
| Histogram / counting / mode | `session2/task3_histogram.c`, `p035`, `p045`, `practice/q25` |
| Prefix sum, compaction, partition, recurrences | `practice/q06`, `p037`–`p041`, `p044` |
| Breaking a dependency (Fibonacci, recurrence) | `p040`, `p041`, `p042` |
| Numerical integration, π, series | `p028`–`p030`, `p027`, `practice/q02`, `practice/q03` |
| Sorting | `session2/task2_sort.c`, `session3/task1_mergesort.c`, `p046`–`p052`, `practice/q28`, `practice/q30` |
| Matrix add/scale/transpose/rotate/flip | `task4_matadd.c`, `session2/task4_transpose.c`, `p055`, `p063`, `p064` |
| Matrix multiply / power / blocks / triangular | `task5_matmul.c`, `practice/q13`, `p065`, `p067`–`p069` |
| Matrix–vector, Aᵀx, sparse | `practice/q05`, `p056`, `p076` |
| Matrix properties, diagonals, row/column stats | `practice/q10`, `practice/q11`, `p058`–`p062` |
| Linear algebra: LU, elimination, determinant, inverse, Jacobi, eigenvalue | `p070`–`p075` |
| `collapse` rules (when it is legal / when it races) | `p066`, `task4_matadd.c` |
| Stencils: convolution, heat, Jacobi, Game of Life, pooling, median | `session2/task5_convolution.c`, `practice/q32`–`q34`, `p079`, `p082`, `p083` |
| Gauss–Seidel, red-black, wavefront / anti-diagonals (lecture slides) | `p077`, `p078`, `p103` |
| Images | `p080`–`p084` |
| Scheduling / uneven work | `session3/task2_schedule.c`, `p030`, `p086`, `p099`, `p100`, `practice/q07`, `practice/q26` |
| Tasks: recursion, cutoff, dependencies, pipelines | `session3/task1_mergesort.c`, `p087`–`p092`, `p104`, `practice/q27`–`q29` |
| Graphs | `p093`–`p095` |
| Synchronisation, locks, deadlock, producer–consumer | `p010`, `p015`–`p017`, `practice/q22`, `practice/q23` |
| Performance effects: false sharing, chunk size, first touch, Amdahl | `p008`, `p009`, `p012`–`p014`, `p019`, `p020` |
| Data-sharing clauses, runtime functions | `p001`–`p007`, `practice/q20`, `practice/q21` |

---

## 3. Build a solution fast from the bank

```bash
cp problems/common.h problems/p0NN_closest.c ~/exam/ && cd ~/exam
mv p0NN_closest.c solution.c
# edit: header (Problem/Solution), serial function, parallel function, main's sizes
gcc -O2 -Wall -Wextra -fopenmp solution.c -o solution -lm && ./solution 1000 && ./solution
```
Or start from `practice/q00_template.c` (self-contained, no `common.h` needed).

Typical edits:
- Different element type: change `double` ↔ `int`/`long`, and the checker (`close_doubles` ↔ `same_ints`).
- Required sizes: change the default `N` or loop over a sizes array:
  `long sizes[] = {100000, 1000000, 10000000};` (copy the loop from `session3/task3_evenodd.c`).
- Required thread counts: edit `THREADS[]` in `common.h`, or the `threads[]` array.
- Thread count via environment instead of code: remove `omp_set_num_threads` and run
  `for t in 1 2 4 8; do OMP_NUM_THREADS=$t ./solution; done`.

---

## 4. README / report template for the submission

```markdown
# <Problem title>
**Machine:** <CPU model>, <cores> cores, gcc <version>  (from lscpu / nproc / gcc --version)
**Build:** gcc -O2 -Wall -Wextra -fopenmp solution.c -o solution -lm
**Run:** ./solution [N]   — thread counts set with omp_set_num_threads() (1, 2, 4, 8)
**Correctness:** every parallel result is compared with the serial one (all OK).

## Approach
<2–4 lines: which loop is parallel, why it is race-free, which clauses (reduction/schedule/...).>

## Results
| N | threads | serial (s) | parallel (s) | speedup | efficiency |
|---|---|---|---|---|---|

## Observations
<2–3 lines per table, using the explanations in section 1, step 7.>

## Answers to the questions
<one short paragraph per question>
```
