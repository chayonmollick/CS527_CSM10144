# OpenMP problem bank — index

104 solved problems, each a complete program in `problems/` with a serial
version, the parallel version(s), a correctness check against serial, a timing table
for 1/2/4/8 threads and viva notes at the end. All include `problems/common.h`.
Their output on an Apple M4 is in `problems/sample_output/`.

Build all: `make -C problems`. Build one: `cd problems && gcc -O2 -Wall -Wextra -fopenmp pNNN_name.c -o pNNN -lm`.
How to use these for a new question: `GUIDE.md`. Matrix questions: `MATRIX_RULES.md`.
Sources: textbook exercises are from *Parallel Programming in MPI and OpenMP* (Eijkhout),
Part II (the book named in the lab handouts); the lecture slides are *ParallelComputation.pptx*.

Also solved elsewhere in the repo: the lab tasks (root = Lab 7, `session2/` = Lab 8,
`session3/` = Lab 9) and `practice/q00`–`q34` (see `practice/README.md`).

## Contents

- [Format example](#format-example) — P000–P000
- [Basics, synchronisation and performance effects](#basics-synchronisation-and-performance-effects) — P001–P020
- [1D arrays: maps, reductions, numerical integration, search](#1d-arrays-maps-reductions-numerical-integration-search) — P021–P036
- [Scans, dependencies, sorting and searching](#scans-dependencies-sorting-and-searching) — P037–P054
- [Matrices and linear algebra](#matrices-and-linear-algebra) — P055–P076
- [Grids, stencils and images](#grids-stencils-and-images) — P077–P086
- [Tasks, recursion, graphs and applications](#tasks-recursion-graphs-and-applications) — P087–P104

## Format example

| # | Problem | File |
|---|---|---|
| P000 | Sum of squares (format example for the problem bank) | [`p000_example.c`](problems/p000_example.c) |

### P000 — Sum of squares (format example for the problem bank)

**Problem.** Given an array a[N] of doubles, compute S = a[0]^2 + a[1]^2 + ... + a[N-1]^2 serially and with OpenMP. Verify the parallel result and report the time, speedup and efficiency for 1, 2, 4 and 8 threads.

**Solution.** Every iteration adds into the same S, so a plain parallel for would race on S. reduction(+:S) gives each thread a private S (starting at 0) and adds the private copies together once at the end.

*Source:* pattern of Lab 7 Task 2.

File: [`problems/p000_example.c`](problems/p000_example.c)

## Basics, synchronisation and performance effects

| # | Problem | File |
|---|---|---|
| P001 | Hello threads and the OpenMP runtime functions | [`p001_hello_threads.c`](problems/p001_hello_threads.c) |
| P002 | Summing thread numbers: the classic race and three fixes | [`p002_tsum_race.c`](problems/p002_tsum_race.c) |
| P003 | Nested parallel regions | [`p003_nested_parallel.c`](problems/p003_nested_parallel.c) |
| P004 | What value does a reduction variable start with inside the region? | [`p004_reduction_identity.c`](problems/p004_reduction_identity.c) |
| P005 | Find the LAST index that satisfies a condition (and the lastprivate trap) | [`p005_lastprivate_search.c`](problems/p005_lastprivate_search.c) |
| P006 | Giving every thread's private variable the same starting value | [`p006_private_init.c`](problems/p006_private_init.c) |
| P007 | master has no barrier: "#pragma omp master a = f();" before a worksharing loop | [`p007_master_vs_single.c`](problems/p007_master_vs_single.c) |
| P008 | Measured speedup, efficiency, Karp-Flatt serial fraction and Amdahl prediction | [`p008_speedup_amdahl.c`](problems/p008_speedup_amdahl.c) |
| P009 | Amdahl's law measured: a program with a tunable serial fraction | [`p009_amdahl_serial_fraction.c`](problems/p009_amdahl_serial_fraction.c) |
| P010 | One sum, four ways to synchronise it: critical, atomic, local+critical, reduction | [`p010_sum_sync_methods.c`](problems/p010_sum_sync_methods.c) |
| P011 | Maximum AND minimum of an array without the reduction clause | [`p011_maxmin_without_reduction.c`](problems/p011_maxmin_without_reduction.c) |
| P012 | False sharing: per-thread counters side by side vs padded vs local | [`p012_false_sharing.c`](problems/p012_false_sharing.c) |
| P013 | Why schedule(static,1) or (dynamic,1) is a bad idea for b[i] = f(a[i]) | [`p013_chunk_size_one.c`](problems/p013_chunk_size_one.c) |
| P014 | First touch: who initialises an array decides where its memory lives | [`p014_first_touch.c`](problems/p014_first_touch.c) |
| P015 | Deadlock from inconsistent lock order, detected safely, and the fix | [`p015_lock_deadlock.c`](problems/p015_lock_deadlock.c) |
| P016 | Bank transfers with one lock per account | [`p016_bank_transfers.c`](problems/p016_bank_transfers.c) |
| P017 | Producer-consumer with a bounded circular buffer | [`p017_producer_consumer.c`](problems/p017_producer_consumer.c) |
| P018 | Parallel random numbers that do not depend on the thread count | [`p018_parallel_random.c`](problems/p018_parallel_random.c) |
| P019 | Sequential code vs single-threaded OpenMP code | [`p019_serial_vs_one_thread.c`](problems/p019_serial_vs_one_thread.c) |
| P020 | SIMD vectorisation with omp simd and parallel for simd | [`p020_simd_vectorise.c`](problems/p020_simd_vectorise.c) |

### P001 — Hello threads and the OpenMP runtime functions

**Problem.** Write a program with a parallel region in which every thread prints "hello" with its thread number. Print omp_get_num_threads(), omp_get_thread_num(), omp_get_max_threads(), omp_get_num_procs() and omp_in_parallel() before, inside and after the region. Run the region with 1, 2, 4, 8 and 2 x (number of cores) threads. Can you ask for more threads than there are cores?

**Solution.** The runtime functions answer "who am I / how big is my team" only inside a region; outside it the program is a team of one. omp_set_num_threads(p) sets the team size of the next regions (dynamic adjustment is switched off so the request is obeyed exactly). Each thread counts its hello with an atomic increment and marks seen[tid], so the check is race-free.

*Source:* textbook Ex 17.1, 17.2, 18.1, 18.3, 18.4, 19.2.

File: [`problems/p001_hello_threads.c`](problems/p001_hello_threads.c)

### P002 — Summing thread numbers: the classic race and three fixes

**Problem.** The code  int tsum = 0;  #pragma omp parallel { tsum += omp_get_thread_num(); } should leave tsum = 0 + 1 + ... + (p-1) = p(p-1)/2. Show that it is wrong (a data race), then fix it with atomic, with critical and with a reduction clause. Verify each fix for p = 2, 4, 8 and 16 threads.

**Solution.** Tsum += id is load / add / store; two threads can load the same old value and one update is lost. The race window is tiny, so the region is run many times with a barrier just before the update (all threads hit it together) and tsum is volatile (gcc must really load and store it). A second racy version widens the window by spinning between the load and the store. Fixes: atomic (one hardware read-modify-write), critical (one thread at a time), reduction(+:tsum) (private copies, combined once at the end).

*Source:* textbook Ex 17.3.

File: [`problems/p002_tsum_race.c`](problems/p002_tsum_race.c)

### P003 — Nested parallel regions

**Problem.** Write a function that contains a parallel region (2 threads). Call it from serial code, and from inside another parallel region (3 threads), first with nested parallelism disabled and then enabled. In each thread print omp_get_level(), omp_get_active_level(), omp_get_ancestor_thread_num() and the team sizes, and check how many threads run at each nesting level.

**Solution.** A parallel region met inside an active region creates a new team only if nesting is allowed (max-active-levels >= 2); otherwise it runs with a team of one (inactive: the level still goes up, the ACTIVE level does not). Each thread records what it saw into its own slot rec[k] (k from an atomic capture) and adds itself to a per-level counter with atomic, so the bookkeeping is race-free. Records are printed after the regions, sorted, so the output is readable.

*Source:* textbook Ex 18.2.

File: [`problems/p003_nested_parallel.c`](problems/p003_nested_parallel.c)

### P004 — What value does a reduction variable start with inside the region?

**Problem.** Show that in reduction(op:x) every thread's private copy of x starts at the identity of op (+:0, \*:1, max:INT_MIN, min:INT_MAX, &&:1, ||:0, &:~0, |:0, ^:0), whatever value x had before the region, and that the original value of x IS combined into the result at the end. Check every operator.

**Solution.** X is set to a non-identity value before each region. Inside, every thread first records its private copy into seen[thread] (its own slot: no race), then combines a contribution of its own. After the region we check (1) all private copies started at the identity and (2) final x = original op all contributions.

*Source:* textbook Ex 20.3.

File: [`problems/p004_reduction_identity.c`](problems/p004_reduction_identity.c)

### P005 — Find the LAST index that satisfies a condition (and the lastprivate trap)

**Problem.** Given int a[N], find the largest index i with cond(a[i]) (here a[i] % 100 == 7), or -1 if there is none. Parallelise the loop  for (i...) if (cond(a[i])) result = i; Show that a shared result is a race, give a critical-section fix, explain why lastprivate(result) with "if (cond) result = i;" is NOT correct, and give a correct fast version. Verify against the serial result and time the correct versions.

**Solution.** (1) shared result: threads overwrite each other; the final value is the last WRITE in time, not the largest index. (2) critical + "keep the larger index": correct, but one lock per match. (3) lastprivate copies out the private value of the thread that ran the sequentially LAST iteration (i = N-1); that thread's copy only knows matches in its own chunk (or is unset), so it fails when the last chunk has no match (dataset B). (4) reduction(max:result): each thread keeps the largest matching index it saw, the maxima are combined at the end: correct for any schedule, no locks.

*Source:* textbook Ex 19.13.

File: [`problems/p005_lastprivate_search.c`](problems/p005_lastprivate_search.c)

### P006 — Giving every thread's private variable the same starting value

**Problem.** Every thread needs its own private copy of x that starts with the same value (42). Show three ways: firstprivate, single + copyprivate, and threadprivate + copyin; also plain assignment inside the region. Print the value each thread sees, say when each method is preferable, and check that all threads saw the value.

**Solution.** Firstprivate copies the value the variable had before the region; copyprivate broadcasts the value ONE thread computed inside the region (the single has an implied barrier, after which every private x holds it); copyin copies the master's threadprivate (global, per-thread) variable to every thread's copy at the start of the region. Each thread writes only got[its id], so recording is race-free.

*Source:* textbook Ex 21.3.

File: [`problems/p006_private_init.c`](problems/p006_private_init.c)

### P007 — master has no barrier: "#pragma omp master a = f();" before a worksharing loop

**Problem.** In #pragma omp parallel {  #pragma omp master a = f(); #pragma omp for for (i = 0; i < N; i++) b[i] = a \* i;  } explain why the loop can use a before it is set. Demonstrate the bug safely (f is slow, the program still terminates), then give correct versions (single; master + barrier) and check them.

**Solution.** Master = "only thread 0 executes this" with NO barrier at entry or exit, and omp for has its barrier at the END, not the start. So threads 1..p-1 run straight into the loop and read a while thread 0 is still inside f(): their part of b uses the old value (here 0). single has an implied barrier at its end, so every thread waits until a is set; master + explicit barrier does the same. (a is volatile in the buggy version only so that the stale reads are really performed and visible.)

*Source:* textbook Ex 21.2.

File: [`problems/p007_master_vs_single.c`](problems/p007_master_vs_single.c)

### P008 — Measured speedup, efficiency, Karp-Flatt serial fraction and Amdahl prediction

**Problem.** Time a loop with a measurable amount of work (many multiples of the timer resolution omp_get_wtick()) for 1, 2, ..., 8 threads. Compute speedup S = T1/Tp, efficiency E = S/p and the experimentally determined serial fraction (Karp-Flatt) e = (1/S - 1/p) / (1 - 1/p). Use e with Amdahl's law to predict the speedup on 16 and 64 threads, and the maximum possible speedup 1/e.

**Solution.** The work is the midpoint rule for pi = integral of 4/(1+x^2) on [0,1]; iterations are independent, the sum uses reduction(+:sum) so there is no race. T1 is the serial program (not the parallel one on 1 thread). For small N the computation is repeated `rounds` times per timing so that every measurement is many ticks long. Amdahl: S(p) = 1 / (e + (1 - e)/p)  <=  1/e  for every p.

*Source:* textbook Ex 28.1; Amdahl's law and Karp-Flatt metric from the lectures.

File: [`problems/p008_speedup_amdahl.c`](problems/p008_speedup_amdahl.c)

### P009 — Amdahl's law measured: a program with a tunable serial fraction

**Problem.** Write a program whose work consists of N equal items; a fraction f of the items must be done by one thread (serial part), the rest in parallel. For f = 0, 0.05, 0.2, 0.5 measure the speedup on 1, 2, 4, 8 threads and print it next to Amdahl's prediction 1 / (f + (1 - f)/p). Check that every run computes the same result as the serial program.

**Solution.** Items 0 .. f\*N-1 are summed by the initial thread before the parallel region (the other threads idle, as in Amdahl's model); items f\*N .. N-1 are summed with parallel for reduction(+:sum). The item is an integer hash (unsigned 64-bit arithmetic wraps, so addition is exactly associative): results must be IDENTICAL.

*Source:* Amdahl's law from the lectures (lab extension).

File: [`problems/p009_amdahl_serial_fraction.c`](problems/p009_amdahl_serial_fraction.c)

### P010 — One sum, four ways to synchronise it: critical, atomic, local+critical, reduction

**Problem.** Compute S = sum_{i<N} sin(i)\*cos(i) in parallel in four ways: (a) critical around every update, (b) atomic on every update, (c) accumulate in a thread-local variable and add it to S with one critical per thread, (d) the reduction clause. Verify each against the serial sum, time them for 1, 2, 4, 8 threads and explain the ordering of the timings.

**Solution.** The term f(i) is computed outside any lock in all versions; only the update of the shared S is protected. (a) and (b) synchronise N times, (c) and (d) only once per thread, so (c) ~ (d) << (b) <= (a) typically. All four are race-free.

*Source:* textbook Ex 23.1 / 20.1.

File: [`problems/p010_sum_sync_methods.c`](problems/p010_sum_sync_methods.c)

### P011 — Maximum AND minimum of an array without the reduction clause

**Problem.** Find max and min of int a[N] in parallel WITHOUT reduction: (a) critical around the comparison for every element, (b) per-thread local max/min, combined with one critical per thread, (c) per-thread results stored in an array indexed by thread id, combined serially, (d) "check before critical": if (a[i] > max) { critical { if (a[i] > max) max = a[i]; } } Explain why (d) needs the re-check inside the critical. Compare all with reduction(max:) / reduction(min:). Verify and time every version.

**Solution.** (a) serialises every element. (b) and (c) do the scan privately and synchronise once per thread. (d) enters the lock only when an element beats the current max, which for random data is rare; the outside read uses atomic read (and the write atomic write) so even the unlocked test is race-free. max and min use different NAMED criticals so they do not block each other.

*Source:* textbook Ex 20.2.

File: [`problems/p011_maxmin_without_reduction.c`](problems/p011_maxmin_without_reduction.c)

### P012 — False sharing: per-thread counters side by side vs padded vs local

**Problem.** Count the multiples of 3 in [0, N) in parallel, each thread counting into its own counter, cnt[id] += (i % 3 == 0) in every iteration: (a) a shared array cnt[thread_id] (counters adjacent in memory), (b) the same array padded so that every counter has its own cache line, (c) a local variable, written to the array once at the end. Verify that all totals are equal and time them for 1, 2, 4, 8 threads. Explain the difference.

**Solution.** There is no race in any version: thread t writes only cnt[t]. But in (a) several counters share one cache line, so every increment invalidates the line in the other cores' caches (false sharing) and the line ping-pongs between cores. (b) pads each counter to its own line; (c) keeps the counter in a register. Counters in (a)/(b) are volatile so that gcc really stores every update to memory (otherwise it would keep them in a register and hide the effect); the update is written branch-free so there is a store in every iteration, as in sum[id] += x[i]. Speedups are relative to the same version on 1 thread (the serial loop is only the correctness reference: some CPUs treat a fixed-address counter specially).

*Source:* false sharing from the lectures / textbook ch. 25 (lab extension).

File: [`problems/p012_false_sharing.c`](problems/p012_false_sharing.c)

### P013 — Why schedule(static,1) or (dynamic,1) is a bad idea for b[i] = f(a[i])

**Problem.** For the cheap loop b[i] = 2\*a[i] + 1, compare the schedules static, static,1, static,8, static,64, dynamic,1 and dynamic,64 on 1, 2, 4, 8 threads. Verify the output is identical for all of them and explain the timings.

**Solution.** Every iteration writes only its own b[i], so all schedules are race-free and give bit-identical results. They differ in cost: with chunk 1 neighbouring elements go to different threads, so all threads write into the same cache lines of b (false sharing), each thread reads every cache line of a but uses only 1 element of it (no locality, no vectorisation), and dynamic,1 adds one trip to the shared scheduler counter per element. Larger chunks (or plain static) avoid all three.

*Source:* textbook Ex 19.7.

File: [`problems/p013_chunk_size_one.c`](problems/p013_chunk_size_one.c)

### P014 — First touch: who initialises an array decides where its memory lives

**Problem.** Arrays a, b, c (N doubles) are used in the parallel loop c[i] = a[i] + 3\*b[i]. Compare (a) initialising the arrays serially, then processing in parallel, with (b) initialising them with the same "parallel for schedule(static)" that later processes them. Also time the very first initialisation of fresh memory (page faults) against a second initialisation of the same memory. Verify the results are equal.

**Solution.** Malloc only reserves address space; a physical page is allocated (and on a NUMA machine placed on the memory of the socket of the touching thread) when it is first WRITTEN. With serial init every page sits next to thread 0's socket and all other threads read remote memory; with parallel static init thread t first-touches exactly the block it will later process, so its pages are local. Both versions are race-free (each i is written by one thread) and give identical c = 2.5\*i.

*Source:* textbook ch. 25 (affinity / first touch).

File: [`problems/p014_first_touch.c`](problems/p014_first_touch.c)

### P015 — Deadlock from inconsistent lock order, detected safely, and the fix

**Problem.** Two sections each need locks A and B. Section 1 takes A then B, section 2 takes B then A. Show that this can deadlock, without ever hanging the program, and fix it. Check that the fixed version completes and the shared data is consistent.

**Solution.** Deadlock = circular wait: section 1 holds A and waits for B while section 2 holds B and waits for A. To demonstrate it safely, each section takes its first lock, waits (with a timeout) until the other also holds its first lock, then tries the second lock with omp_test_lock() in a loop with a time limit instead of blocking in omp_set_lock(). Giving up = "would have deadlocked". The fix: every thread acquires the locks in the SAME global order (A before B); then no cycle is possible. The fixed version moves money between two balances X (lock A) and Y (lock B) many times.

*Source:* textbook Ex 23.2.

File: [`problems/p015_lock_deadlock.c`](problems/p015_lock_deadlock.c)

### P016 — Bank transfers with one lock per account

**Problem.** There are M accounts and T transfers (from, to, amount). Execute the transfers in parallel using one omp_lock_t per account. Avoid deadlock and skip transfers with from == to. Check that the total money is unchanged and that the final balances equal the serial result. Compare the time with one global critical.

**Solution.** A transfer locks both of its accounts. Two transfers i->j and j->i taking "from first" would deadlock, so locks are always acquired in index order (lower account first): no cycle of waiting threads is possible. from == to is skipped (it would try to take the same lock twice and block forever). Transfers on different accounts run in parallel; the global critical serialises all of them. Each transfer is two integer additions, which commute, so any order gives the serial balances. Every transfer also does W units of "processing" while holding its locks (stands for validation/logging; the amount moved depends on it, deterministically per k). With W = 0 the locked work is ~1 ns and lock overhead dominates every version.

*Source:* lab extension of textbook ch. 23 (locks).

File: [`problems/p016_bank_transfers.c`](problems/p016_bank_transfers.c)

### P017 — Producer-consumer with a bounded circular buffer

**Problem.** Producers put the numbers 1..N into a circular buffer of size B; consumers take them out and add them up. Implement it (a) for P producers and C consumers with critical sections, (b) for one producer section and one consumer section that synchronise through an atomic counter and flush. Check that the sum is N(N+1)/2 and that every item is consumed exactly once. Explain why a busy-wait loop needs flush / atomic reads.

**Solution.** (a) every buffer operation (test count, move head/tail, change count) is one critical(buffer) block; a thread that finds the buffer full/empty leaves the critical and tries again. Entering/leaving a critical implies a flush, so no extra flushes are needed. Consumers stop when the buffer is empty and all N items have been taken. (b) only count is shared by both sides: it is read/updated with atomic. The producer writes the slot, flushes, then increments count; the consumer sees count > 0, flushes, then reads the slot (and flushes again before decrementing, so the slot is not overwritten while being read). seen[item]++ is written by exactly one consumer per item (items are unique), so the check array is race-free.

*Source:* lab extension of textbook ch. 23 / 26 (locks, flush, memory model).

File: [`problems/p017_producer_consumer.c`](problems/p017_producer_consumer.c)

### P018 — Parallel random numbers that do not depend on the thread count

**Problem.** Fill an array with N pseudo-random numbers in [0,1) in parallel so that the array is identical for 1, 2, 4 and 8 threads. Contrast with per-thread seeded generators and explain why rand() must not be used. Use the generator to estimate pi by Monte Carlo with M points. Check that the arrays are bitwise identical for all thread counts and that the estimate is within 0.01 of pi.

**Solution.** A counter-based generator: x_i = hash(seed, i) (the splitmix64 mixer applied to seed + (i+1)\*golden). Element i depends only on i, never on which thread computes it or what was computed before, so any schedule and any thread count give the same bits, and there is no shared generator state (no race). Per-thread LCGs (seed + id, one block each) are also race-free and fast, but the numbers change with the thread count. Pi: point j = (x_2j, x_2j+1), count points with x^2 + y^2 <= 1 using reduction(+:inside); the integer count, hence the estimate, is identical for all p.

*Source:* lab extension (Monte Carlo pi, random numbers in parallel).

File: [`problems/p018_parallel_random.c`](problems/p018_parallel_random.c)

### P019 — Sequential code vs single-threaded OpenMP code

**Problem.** Time the loop a[i] = sqrt(b[i]) + c[i]\*d (i = 0..N-1) written as plain sequential code and as an OpenMP "parallel for" run with ONE thread. Check that both produce the same a[], print both times and their ratio, and explain the difference (parallel-region overhead, the outlined function, optimisation level).

**Solution.** Iterations are independent (a[i] needs only b[i] and c[i]), so a plain "#pragma omp parallel for" is race-free. With 1 thread the arithmetic is the same, so any extra time is OpenMP overhead: starting/joining the team, computing the thread's iteration range, and running the body as an "outlined" function (kernel_omp._omp_fn.0) that reaches a, b, c, d through a struct of pointers, so gcc may not inline it, propagate constants or vectorise it as well. The overhead is a fixed cost per region: it shows up for a small N called many times and disappears for a large N.

*Source:* textbook Exercise 19.6.

File: [`problems/p019_serial_vs_one_thread.c`](problems/p019_serial_vs_one_thread.c)

### P020 — SIMD vectorisation with omp simd and parallel for simd

**Problem.** For SAXPY (z = a\*x + y) and the dot product s = x . y on arrays of N doubles, compare four versions: a plain loop, "#pragma omp simd" (one thread, vector instructions), "#pragma omp parallel for" (threads) and "#pragma omp parallel for simd" (threads + vectors); use simd reduction(+:s) for the dot product. Verify every version and explain SIMD vs threads.

**Solution.** SAXPY iterations are independent, so both threads and vector lanes can process different i at the same time. The dot product has a loop-carried sum, so it needs reduction(+:s): with simd each vector lane keeps its own partial sum, with parallel for each thread does, and with parallel for simd both (thread partials of lane partials), combined at the end. "omp simd" tells the compiler the loop is safe to vectorise (no aliasing, reordering the sum is allowed).

File: [`problems/p020_simd_vectorise.c`](problems/p020_simd_vectorise.c)

## 1D arrays: maps, reductions, numerical integration, search

| # | Problem | File |
|---|---|---|
| P021 | Element-wise array operations, separate loops vs one fused loop | [`p021_elementwise_ops.c`](problems/p021_elementwise_ops.c) |
| P022 | Linear combination of three vectors | [`p022_linear_combination.c`](problems/p022_linear_combination.c) |
| P023 | Vector norms in one parallel loop | [`p023_vector_norms.c`](problems/p023_vector_norms.c) |
| P024 | Count elements above the average in one parallel region | [`p024_above_average.c`](problems/p024_above_average.c) |
| P025 | Count positives, negatives, zeros and values in a range | [`p025_count_categories.c`](problems/p025_count_categories.c) |
| P026 | Product of all elements modulo 1e9+7 | [`p026_product_mod.c`](problems/p026_product_mod.c) |
| P027 | Series sums and floating-point summation order | [`p027_series_sums.c`](problems/p027_series_sums.c) |
| P028 | pi from the area of a quarter circle | [`p028_pi_quarter_circle.c`](problems/p028_pi_quarter_circle.c) |
| P029 | Numerical integration: composite trapezoidal and Simpson's rule | [`p029_trapezoid_simpson.c`](problems/p029_trapezoid_simpson.c) |
| P030 | Integration with uneven work per interval (loop schedules) | [`p030_adaptive_integration.c`](problems/p030_adaptive_integration.c) |
| P031 | Loop with an index conflict: x[i/2] += f(i) | [`p031_index_conflict.c`](problems/p031_index_conflict.c) |
| P032 | Smallest nonzero element (min reduction and a user-defined reduction) | [`p032_smallest_nonzero.c`](problems/p032_smallest_nonzero.c) |
| P033 | Top 3 largest elements with their indices | [`p033_top_k.c`](problems/p033_top_k.c) |
| P034 | Is the array sorted? Count descents; is it a palindrome? | [`p034_sorted_check.c`](problems/p034_sorted_check.c) |
| P035 | Mode and number of distinct values via a parallel histogram | [`p035_mode_distinct.c`](problems/p035_mode_distinct.c) |
| P036 | Parallel search with early exit | [`p036_search_early_exit.c`](problems/p036_search_early_exit.c) |

### P021 — Element-wise array operations, separate loops vs one fused loop

**Problem.** Given arrays a[N] and b[N], compute element by element c = a\*b (Hadamard product), d = sqrt(|a|) + exp(-b\*b) and e = max(a, b). Parallelise each operation with its own parallel for, then compute all three in ONE fused loop. Verify both against serial code and compare the fused and separate versions (memory traffic).

**Solution.** Element i of c, d, e depends only on a[i] and b[i], so every loop is a plain "#pragma omp parallel for" with no shared writes (each i is written by one thread). Separate loops read a and b three times and start three parallel regions; the fused loop reads a[i], b[i] once (kept in registers) and writes c, d, e in a single pass: 5N instead of 9N doubles of memory traffic.

File: [`problems/p021_elementwise_ops.c`](problems/p021_elementwise_ops.c)

### P022 — Linear combination of three vectors

**Problem.** Given vectors x, y, w of length N and scalars alpha, beta, gamma, compute z = alpha\*x + beta\*y + gamma\*w and the 3-vector sum s = x + y + w, serially and with OpenMP. Verify both and explain their memory-bound behaviour (bytes moved per floating-point operation).

**Solution.** Z[i] and s[i] depend only on x[i], y[i], w[i], so each loop is a plain "#pragma omp parallel for" (each i is written by exactly one thread, inputs are only read: race-free). Each element needs 3 loads + 1 store = 32 bytes for 5 flops (z) or 2 flops (s), so the speed is set by memory bandwidth, not by the arithmetic; the program also prints the achieved bandwidth in GB/s.

File: [`problems/p022_linear_combination.c`](problems/p022_linear_combination.c)

### P023 — Vector norms in one parallel loop

**Problem.** For a vector x[N] of doubles compute the L1 norm sum|x[i]|, the L2 norm sqrt(sum x[i]^2), the L-infinity norm max|x[i]| and the smallest absolute value min|x[i]|, all in ONE parallel loop. Verify against a serial loop and report the speedup.

**Solution.** Each quantity is a reduction, so one loop carries several reduction clauses: reduction(+:l1, sq) reduction(max:mx) reduction(min:mn). Every thread gets private copies initialised to the identity of the operator (0 for +, the smallest double for max, the largest for min) and OpenMP combines them at the end; the square root is taken once, after the loop.

File: [`problems/p023_vector_norms.c`](problems/p023_vector_norms.c)

### P024 — Count elements above the average in one parallel region

**Problem.** Given an integer array a[N], compute the average of its elements and then count how many elements are strictly greater than the average. Do both steps inside ONE parallel region, explain why a barrier is needed between them, and verify against serial code.

**Solution.** One "#pragma omp parallel" region holding two worksharing loops. Loop 1: "#pragma omp for reduction(+:sum)". Its implicit barrier guarantees the combined sum is complete before anyone uses it. Then "#pragma omp single" computes avg once (its own implicit barrier makes avg visible to all threads); variant B lets every thread compute its own private avg instead (no single). Loop 2: "#pragma omp for reduction(+:count)". sum, avg and count are shared (declared before the region); the loop indices are private.

File: [`problems/p024_above_average.c`](problems/p024_above_average.c)

### P025 — Count positives, negatives, zeros and values in a range

**Problem.** Given an integer array a[N] and a range [lo, hi], count in ONE parallel loop how many elements are positive, negative and zero, and how many lie in [lo, hi]. Check that pos + neg + zero == N and verify against serial code.

**Solution.** Four independent counters, each a sum, so one clause reduction(+:pos, neg, zero, inrange) gives every thread private counters starting at 0 and adds them up at the end. Without it, cnt++ on shared counters is a read-modify-write race and counts are lost.

File: [`problems/p025_count_categories.c`](problems/p025_count_categories.c)

### P026 — Product of all elements modulo 1e9+7

**Problem.** Given an array a[N] of integers in 1..1e6, compute the product of all elements modulo p = 1e9+7 in parallel, and also log(product) = sum log(a[i]) as a floating-point check (how many digits the true product has). Verify against serial code.

**Solution.** Reduction(\*:r) cannot be used: the true product overflows at once and the built-in reduction cannot apply "% p" when combining. Two fixes: (A) manual reduction: each thread multiplies mod p into a private value over its share of the loop ("omp for nowait"), then the partial products are combined one thread at a time in a critical section; (B) a user-defined reduction: declare reduction(mulmod : long : omp_out = omp_out \* omp_in % MOD) initializer(omp_priv = 1). Multiplication mod p is associative and commutative, so both give the exact serial answer. The log sum is an ordinary reduction(+:s).

File: [`problems/p026_product_mod.c`](problems/p026_product_mod.c)

### P027 — Series sums and floating-point summation order

**Problem.** For i = 1..N compute three partial sums with an OpenMP reduction: the Basel sum S1 = sum 1/i^2 (-> pi^2/6), the Leibniz series S2 = 4 \* sum_{k=0}^{N-1} (-1)^k/(2k+1) (-> pi) and the harmonic number H_N = sum 1/i (~ ln N + 0.5772...). Show how the summation order changes the last digits (serial forward, serial backward, parallel with 1/2/4/8 threads), explain why parallel floating-point sums are compared with a tolerance, verify within a tolerance and report the speedup.

**Solution.** Each term depends only on i, so one loop with reduction(+:basel, leib, harm) works: each thread adds its own block of terms into private sums which are then added together. The sign (-1)^k is computed from i, NOT by flipping a variable each iteration (that would be a loop-carried dependence). Because floating-point + is not associative, a different grouping rounds differently: results agree only within the rounding-error bound for adding N numbers (about N \* DBL_EPSILON relative), not bit for bit.

File: [`problems/p027_series_sums.c`](problems/p027_series_sums.c)

### P028 — pi from the area of a quarter circle

**Problem.** The quarter circle y = sqrt(1 - x^2), 0 <= x <= 1, has area pi/4. Approximate it by the left Riemann sum pi/4 ~ sum_{i=0}^{N-1} dx\*sqrt(1 - x_i^2) with x_i = i\*dx, dx = 1/N. Parallelise the sum with OpenMP, print the error of the estimate for N = 1e3 .. 1e8 and the timing table, and verify the parallel result against the serial one.

**Solution.** The terms are independent; x_i is computed from i (not by x += dx, which would be a loop-carried dependence and also accumulate rounding error). Each thread sums its block of terms into a private copy of s via reduction(+:s); the copies are added at the end. dx is multiplied in once after the loop.

*Source:* textbook Exercise 19.4.

File: [`problems/p028_pi_quarter_circle.c`](problems/p028_pi_quarter_circle.c)

### P029 — Numerical integration: composite trapezoidal and Simpson's rule

**Problem.** Integrate f(x) = exp(-x^2)\*sin(x) + 1 (or any given f) over [a, b] with n subintervals using the composite trapezoidal rule and Simpson's rule (weights 1, 4, 2, 4, ..., 2, 4, 1; n even), serially and with OpenMP. Verify the parallel results against serial ones, check the accuracy on the integral of sin x over [0, pi] (= 2), and report the speedup.

**Solution.** With h = (b - a)/n and x_i = a + i\*h (computed from i), each interior point contributes w_i \* f(x_i) independently, so the sum over i = 1..n-1 is a reduction(+:s). Trapezoid: T = h\*(f(a)/2 + sum f(x_i) + f(b)/2). Simpson: S = h/3\*(f(a) + sum w_i f(x_i) + f(b)) with w_i = 4 for odd i, 2 for even i; the weight depends only on i, so iterations stay independent.

File: [`problems/p029_trapezoid_simpson.c`](problems/p029_trapezoid_simpson.c)

### P030 — Integration with uneven work per interval (loop schedules)

**Problem.** Integrate f(x) = sqrt(1 - x^2) over [0, 1] (= pi/4) using N coarse intervals, where each interval is refined into more sub-steps the steeper f is on it: interval i gets m_i = 1 + K\*|slope_i| midpoint sub-steps. f is very steep near x = 1, so the work per iteration is unpredictable and concentrated at the end. Parallelise the loop over intervals and compare schedule(static), schedule(dynamic, c) and schedule(guided); verify all results agree within a tolerance and explain why dynamic scheduling helps.

**Solution.** Iteration i computes the area of interval i on its own (its own m_i and sub-step loop), and the areas are added with reduction(+:sum), so every schedule is race-free. static hands each thread one contiguous block of N/T intervals, so the thread owning the end of [0, 1] gets most of the sub-steps and the others wait at the barrier. dynamic,c hands out chunks of c intervals on demand and guided hands out shrinking chunks, so the work is balanced.

*Source:* textbook Exercise 19.9.

File: [`problems/p030_adaptive_integration.c`](problems/p030_adaptive_integration.c)

### P031 — Loop with an index conflict: x[i/2] += f(i)

**Problem.** Explain why the loop for (i = 0; i < n; i++) x[i/2] += f(i); cannot be parallelised as written. Demonstrate the race, then rewrite it so that it is parallel and correct, and also give a version with atomic updates. Verify both against the serial loop and compare their times.

**Solution.** Iterations 2k and 2k+1 both update x[k] (read-modify-write), so if they run on different threads one update can be lost: a data race. Fix 1 (owner computes): loop over the OUTPUT index j and let iteration j do both updates, x[j] += f(2j) + f(2j+1); every x[j] is written by exactly one iteration, so a plain parallel for is race-free (and needs no synchronisation). Fix 2: keep the loop over i and make each update "#pragma omp atomic" (correct, but every update pays for an atomic instruction). f returns integers, so results compare exactly.

*Source:* textbook Exercise 19.3.

File: [`problems/p031_index_conflict.c`](problems/p031_index_conflict.c)

### P032 — Smallest nonzero element (min reduction and a user-defined reduction)

**Problem.** Given an array of nonnegative ints, return its smallest NONZERO element, or -1 if the array is empty or all its elements are zero. Parallelise with OpenMP in two ways: a built-in reduction(min:) and a user-defined reduction. Test a normal array, an all-zero array, an empty array and an array with a single nonzero element; verify against serial code and time both versions.

**Solution.** (A) reduction(min:m) with m starting at INT_MAX ("no nonzero yet") and only nonzero elements taking part; since INT_MAX could itself be the answer, a second reduction(+:nonzeros) tells "no nonzero at all" apart. (B) declare reduction(minnz : int : ...) with the operator minnz(x, y) = smaller of x and y ignoring zeros. Its identity is 0 ("nothing seen yet"), so private copies start at 0 (initializer(omp_priv = 0)) and the result 0 means "no nonzero": no sentinel.

*Source:* textbook Exercise 20.4.

File: [`problems/p032_smallest_nonzero.c`](problems/p032_smallest_nonzero.c)

### P033 — Top 3 largest elements with their indices

**Problem.** Find the 3 largest elements of an integer array a[N] together with their indices (largest first; equal values: smaller index first) using OpenMP. Verify against a serial scan and against sorting a copy of the array, and time it.

**Solution.** Each thread scans its part of the array ("omp for nowait") keeping a private sorted top-3 list (insertion into 3 slots). The per-thread lists are then merged into the global list, either (A) inside a critical section (one thread at a time) or (B) by storing each list at lists[thread id] and merging serially after the region. Because the order "larger value, then smaller index" is a strict total order, the merged answer does not depend on the merge order.

File: [`problems/p033_top_k.c`](problems/p033_top_k.c)

### P034 — Is the array sorted? Count descents; is it a palindrome?

**Problem.** For an integer array a[N], use OpenMP to (1) check whether it is sorted in ascending (non-decreasing) order, (2) count the "descents" i with a[i] > a[i+1], and (3) check whether it is a palindrome (a[i] == a[N-1-i] for all i). Test a sorted array, one that is unsorted only at the end, a palindrome and a random array; verify against serial code and time the checks.

**Solution.** Each test is a reduction over independent pairs. Sortedness: reduction(&&:sorted) (private copies start at 1 = true, combined with &&); descents in the same loop with reduction(+:descents). Iteration i only READS a[i] and a[i+1], so the pair that straddles two threads' blocks is still checked and there is no race. Palindrome: reduction(&&:palin) over i < N/2 comparing a[i] with a[N-1-i]. Consistency: sorted must equal (descents == 0).

File: [`problems/p034_sorted_check.c`](problems/p034_sorted_check.c)

### P035 — Mode and number of distinct values via a parallel histogram

**Problem.** Given an array a[N] of integers in a small range [0, R), find the mode (the most frequent value; ties go to the smallest value) and the number of distinct values, using OpenMP. Build a histogram in parallel, then find the mode and count the distinct values from it in parallel. Verify against serial code and time it.

**Solution.** Histogram, two ways: (A) reduction(+:h[:R]), an OpenMP 4.5 array-section reduction: every thread gets a private zeroed copy of h[0..R-1], adds into it, and the copies are summed at the end; (B) the same by hand: a private histogram per thread ("omp for nowait") added into the shared one inside a critical section. Then over the R bins: reduction(max:maxc) and reduction(+:distinct) in one loop, and reduction(min:mode) over the bins with h[v] == maxc (smallest such value = tie rule). An argmax needs two passes because a built-in reduction cannot carry the (count, value) pair.

File: [`problems/p035_mode_distinct.c`](problems/p035_mode_distinct.c)

### P036 — Parallel search with early exit

**Problem.** Decide whether a value occurs in an integer array a[N] (and where). Compare four OpenMP versions: (1) a full scan with reduction(min:index) giving the FIRST occurrence, (2) a shared "found" flag checked at the start of every iteration, (3) "#pragma omp cancel for" with "#pragma omp cancellation point for", and (4) a hand-written SPMD loop over chunks that stops early. Time them for a target near the start, in the middle, near the end and absent. Verify the existence answer, and that the full scan returns the first occurrence.

**Solution.** Break is not allowed inside "omp for", so early exit needs other means. (1) every thread scans its block, min reduction keeps the smallest index: always scans all N elements. (2) the flag is written/read with omp atomic; iterations still run but skip the work once it is set, so it only helps if the work per iteration is large. (3) cancel for makes the threads leave the loop at their next cancellation point; it only works if OMP_CANCELLATION=true is set BEFORE the program starts (omp_get_cancellation() tells); otherwise it runs as a full scan. (4) thread t scans chunks t, t+T, t+2T, ... (all threads sweep from the front together) and checks the flag before each chunk, so it stops soon after any thread finds the value. (2)-(4) return AN occurrence, not necessarily the first.

File: [`problems/p036_search_early_exit.c`](problems/p036_search_early_exit.c)

## Scans, dependencies, sorting and searching

| # | Problem | File |
|---|---|---|
| P037 | Exclusive prefix sum and prefix maximum (blocked two-pass scan) | [`p037_exclusive_scan_prefix_max.c`](problems/p037_exclusive_scan_prefix_max.c) |
| P038 | Stream compaction (parallel filter that keeps the order) | [`p038_stream_compaction.c`](problems/p038_stream_compaction.c) |
| P039 | Stable partition (evens first, then odds, both in original order) | [`p039_stable_partition.c`](problems/p039_stable_partition.c) |
| P040 | Loop-carried dependencies: classify them and parallelise correctly | [`p040_loop_dependencies.c`](problems/p040_loop_dependencies.c) |
| P041 | First-order linear recurrence x[i] = c[i]\*x[i-1] + d[i] (affine scan) | [`p041_linear_recurrence.c`](problems/p041_linear_recurrence.c) |
| P042 | Fibonacci array F[0..N] mod 1e9+7 (breaking a recurrence) | [`p042_fibonacci_array.c`](problems/p042_fibonacci_array.c) |
| P043 | Polynomial evaluation (many points; one huge polynomial) | [`p043_polynomial_eval.c`](problems/p043_polynomial_eval.c) |
| P044 | Run-length encoding (count runs; encode as (value, length) pairs) | [`p044_run_length.c`](problems/p044_run_length.c) |
| P045 | Histogram of doubles into B bins, then the cumulative histogram (CDF) | [`p045_histogram_cdf.c`](problems/p045_histogram_cdf.c) |
| P046 | Parallel LSD radix sort of unsigned 32-bit keys (8-bit digits) | [`p046_radix_sort.c`](problems/p046_radix_sort.c) |
| P047 | Bitonic sort (sorting network), parallel compare-exchange stages | [`p047_bitonic_sort.c`](problems/p047_bitonic_sort.c) |
| P048 | Shell sort with the gap subsequences sorted in parallel | [`p048_shell_sort.c`](problems/p048_shell_sort.c) |
| P049 | Parallel bucket sort of doubles uniform in [0, 1) | [`p049_bucket_sort.c`](problems/p049_bucket_sort.c) |
| P050 | k-th smallest element without sorting (quickselect, radix select, bisection) | [`p050_kth_smallest.c`](problems/p050_kth_smallest.c) |
| P051 | Parallel merge of two sorted arrays (co-rank / merge path) | [`p051_parallel_merge.c`](problems/p051_parallel_merge.c) |
| P052 | Merge sort with tasks where the MERGE is parallel too (Lab 9 extension) | [`p052_mergesort_parallel_merge.c`](problems/p052_mergesort_parallel_merge.c) |
| P053 | Sort every row (and every column) of a matrix in parallel | [`p053_sort_rows.c`](problems/p053_sort_rows.c) |
| P054 | Search a value in a matrix: count, first occurrence, list of positions | [`p054_matrix_search.c`](problems/p054_matrix_search.c) |

### P037 — Exclusive prefix sum and prefix maximum (blocked two-pass scan)

**Problem.** Given an int array a[N], compute (a) the exclusive prefix sum out[0] = 0, out[i] = a[0] + a[1] + ... + a[i-1], and (b) the running maximum (prefix max) m[i] = max(a[0], ..., a[i]). Parallelise both with OpenMP, verify exactly against the serial loops and report the time, speedup and efficiency for 1, 2, 4 and 8 threads.

**Solution.** Out[i] depends on out[i-1] (loop-carried), so a plain parallel for is wrong. Blocked two-pass scan in ONE parallel region: each thread takes a contiguous block [lo, hi), pass 1 computes its block total (sum or max), barrier, a single thread turns the totals into block offsets (exclusive scan over threads; identity 0 for +, INT_MIN for max), implicit barrier, pass 2 rescans the block starting from its offset. Each thread writes only its own block and the offsets are read only after the barriers -> race-free. Any associative operator (+, max, min, \*, ...) works the same way.

*Source:* lab extension of the prefix-sum task (practice q06).

File: [`problems/p037_exclusive_scan_prefix_max.c`](problems/p037_exclusive_scan_prefix_max.c)

### P038 — Stream compaction (parallel filter that keeps the order)

**Problem.** Given an int array a[N], copy the elements that satisfy a predicate (here: a[i] % 3 == 0) into a new array out[], in their original order, and return how many were copied. Parallelise with OpenMP, verify against the serial filter and report time, speedup and efficiency for 1, 2, 4, 8 threads.

**Solution.** The output position of a kept element = number of kept elements before it, which is a prefix sum of the 0/1 "keep" flags. Blocked two-pass in one parallel region: pass 1, each thread counts the kept elements of its block [lo, hi); barrier; single does an exclusive scan of the counts -> start[t] (and the total); implicit barrier; pass 2, each thread re-reads its block and writes kept elements to out[start[t]], out[start[t]+1], ... Threads write disjoint slices of out and blocks are in thread order, so the result is race-free and in the original order. No atomics, no locks.

*Source:* lab extension (scan application: filter / pack / compact).

File: [`problems/p038_stream_compaction.c`](problems/p038_stream_compaction.c)

### P039 — Stable partition (evens first, then odds, both in original order)

**Problem.** Rearrange an int array a[N] so that all even numbers come first and all odd numbers after them, each group keeping its original relative order (stable partition). Parallelise with OpenMP, verify against the serial version and report time, speedup and efficiency for 1, 2, 4, 8 threads.

**Solution.** Two stream compactions at once. Pass 1: each thread counts the evens E[t] and odds O[t] of its block [lo, hi). Barrier. single scans the counts: evens of thread t start at E[0]+...+E[t-1]; odds of thread t start at (total evens) + O[0]+...+O[t-1]. Implicit barrier. Pass 2: each thread writes its evens and odds to its own two disjoint slices of out[], in order -> stable and race-free. Out of place (out[]), copy back if needed.

*Source:* lab extension (scan application: split / partition step of radix sort).

File: [`problems/p039_stable_partition.c`](problems/p039_stable_partition.c)

### P040 — Loop-carried dependencies: classify them and parallelise correctly

**Problem.** For each loop below, say whether it has a loop-carried dependency and of which kind, then give a correct OpenMP version, verify it against the serial loop and report time and speedup for 1, 2, 4, 8 threads: (a) for i = 1..N-1:  a[i] = a[i-1] + b[i] (b) for i = 0..N-2:  a[i] = a[i+1] + c (c) for i = 0..N-1:  a[i] = 2\*b[i];  c[i] = a[i] + 1 (d) for i = k..N-1:  a[i] = a[i-k] + b[i]      (dependence distance k) (e) for i = 1..N-1:  a[i] = 2\*b[i];  c[i] = a[i-1] + 1

**Solution.** (a) flow (true, read-after-write) dependence, distance 1: it is a prefix sum, a[i] = a[0] + b[1] + ... + b[i] -> blocked two-pass scan. (b) anti (write-after-read) dependence: iteration i must read the OLD a[i+1] -> copy a to old[] first, then a[i] = old[i+1] + c is independent. (c) only a dependence inside one iteration (not carried) -> plain parallel for, the fused loop is fine. (d) flow dependence at distance k: iterations i, i+k, i+2k, ... form k independent chains -> parallel for over the chains (at most k-way parallel), or blocks of k consecutive iterations are independent -> serial loop over blocks, omp for inside (a barrier per block). (e) carried flow dependence between two statements -> loop fission: first loop computes all a, implicit barrier, second loop all c. The naive `parallel for` versions of (a) and (b) are run to show the race.

*Source:* lecture slides (data dependence analysis); lab extension.

File: [`problems/p040_loop_dependencies.c`](problems/p040_loop_dependencies.c)

### P041 — First-order linear recurrence x[i] = c[i]\*x[i-1] + d[i] (affine scan)

**Problem.** Given c[N] and d[N], compute x[0] = d[0] and x[i] = c[i]\*x[i-1] + d[i] for i = 1..N-1, serially and with OpenMP. Verify within a tolerance and report the time, speedup and efficiency for 1, 2, 4 and 8 threads. (Use |c[i]| <= 1 so x stays bounded.)

**Solution.** Step i is the affine map f_i(x) = c[i]\*x + d[i]; x[i] = f_i(x[i-1]) with x[-1] = 0. Composing two affine maps gives an affine map: apply (A,B) then (c,d):  x -> c\*(A\*x + B) + d = (c\*A)\*x + (c\*B + d). Composition is associative, so the recurrence is a SCAN over maps. Blocked two-pass scan in one parallel region: pass 1, each thread composes the maps of its block [lo, hi) into one map (A_t, B_t) (block in -> block out); barrier; single runs the p block maps in order to get the value entering each block, xin[t] = x[lo_t - 1]; implicit barrier; pass 2, each thread replays the recurrence over its block starting from xin[t]. Each thread writes only x[lo..hi) and the shared xin[] is read after the barriers -> race-free.

*Source:* lab extension (generalised prefix sum; prefix sum is c[i] = 1).

File: [`problems/p041_linear_recurrence.c`](problems/p041_linear_recurrence.c)

### P042 — Fibonacci array F[0..N] mod 1e9+7 (breaking a recurrence)

**Problem.** Fill F[0..N] with the Fibonacci numbers modulo P = 1e9+7 (F[0] = 0, F[1] = 1, F[i] = F[i-1] + F[i-2]). The recurrence is sequential; parallelise it with OpenMP in two ways: (a) compute every F(i) independently, (b) split the index range into blocks. Verify against the serial loop and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Fast doubling gives (F(n), F(n+1)) directly in O(log n) steps: F(2m) = F(m)\*(2F(m+1) - F(m)),  F(2m+1) = F(m)^2 + F(m+1)^2  (all mod P). (a) parallel for over i, F[i] = fib_pair(i): no iteration reads another's result, so there is no dependence at all, but the total work is O(N log N) instead of O(N). (b) one parallel region, each thread takes a block [lo, hi), jumps to (F(lo), F(lo+1)) with fast doubling, then runs the cheap recurrence over its own block: O(N + p log N) work. Every thread writes only its own F[i]s -> race-free.

*Source:* lab extension (Lab 9 / q27 compute Fibonacci with tasks; this is the array version).

File: [`problems/p042_fibonacci_array.c`](problems/p042_fibonacci_array.c)

### P043 — Polynomial evaluation (many points; one huge polynomial)

**Problem.** (a) Evaluate a polynomial of degree D, p(x) = a[0] + a[1]x + ... + a[D]x^D, at M points x[0..M-1]. (b) Evaluate ONE polynomial of huge degree (D = 1e7) at a single point x. Parallelise both with OpenMP, verify against serial Horner within a tolerance and report time and speedup for 1, 2, 4 and 8 threads. Use |x| <= 1 so the powers stay bounded.

**Solution.** (a) Horner for one point is a sequential chain, but different points are independent -> parallel for over the points (embarrassingly parallel, each iteration writes only y[j]). (b) only one point, so split the COEFFICIENTS: p(x) = sum over blocks of x^lo \* q_t(x) with q_t(x) = a[lo] + a[lo+1]x + ... + a[hi-1]x^(hi-1-lo). Each thread runs Horner on its block [lo, hi), multiplies by x^lo (pow) and the block results are added with reduction(+:p) -> race-free.

*Source:* lab extension (Horner's rule; reduction with a per-thread prologue).

File: [`problems/p043_polynomial_eval.c`](problems/p043_polynomial_eval.c)

### P044 — Run-length encoding (count runs; encode as (value, length) pairs)

**Problem.** A run is a maximal block of equal adjacent values. Given an int array a[N], (a) count the runs and (b) produce the run-length encoding: arrays val[] and len[] with one (value, length) pair per run, in order. Parallelise both with OpenMP, verify against the serial RLE and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** I starts a run iff i == 0 or a[i] != a[i-1]; this test reads only the input, so it is independent per i. (a) runs = number of run starts -> parallel for with reduction(+:runs). (b) a run's output slot = number of run starts before it (scan of the start marks). Blocked two-pass in one parallel region: pass 1, each thread counts the starts in its block; barrier; single scans the counts -> first slot of each thread (and the total R, plus the sentinel start[R] = N); implicit barrier; pass 2, each thread writes val[r] = a[i] and start[r] = i for its starts; barrier; then omp for: len[r] = start[r+1] - start[r]. Each thread writes its own slots -> race-free, and the runs come out in order. A run that crosses a block boundary is counted once: only the block holding its first element sees a start there.

*Source:* lab extension (scan application, like stream compaction P038).

File: [`problems/p044_run_length.c`](problems/p044_run_length.c)

### P045 — Histogram of doubles into B bins, then the cumulative histogram (CDF)

**Problem.** Given N doubles in [lo, hi), count how many fall into each of B equal-width bins (bin = (int)((x - lo) / width), width = (hi - lo) / B; clamp so that rounding never gives bin B). Then compute the cumulative histogram cum[b] = h[0] + ... + h[b] and the CDF cum[b] / N. Parallelise with OpenMP in two ways, verify against the serial histogram and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** H[bin]++ from several threads is a race (two threads can hit the same bin). (1) reduction(+:h[:B]) (OpenMP 4.5 array section): every thread gets a private zeroed copy of h, the copies are added at the end. (2) the same done by hand: each thread fills its own local histogram (omp for nowait), then adds it into the shared h with one atomic per bin (B atomics per thread instead of N). The CDF is a prefix sum over only B numbers, so it is done serially (for huge B use the blocked scan of P037).

*Source:* lab extension (Lab 8 histogram task; reduction on arrays).

File: [`problems/p045_histogram_cdf.c`](problems/p045_histogram_cdf.c)

### P046 — Parallel LSD radix sort of unsigned 32-bit keys (8-bit digits)

**Problem.** Sort N unsigned 32-bit integers with an LSD radix sort using 8-bit digits (4 passes), parallelised with OpenMP. Verify that the output is sorted and identical to qsort's, and compare the time with qsort and with the serial radix sort for N = 1e5, 1e6 and 1e7 on 1, 2, 4, 8 threads.

**Solution.** Each pass is a stable counting sort on one digit. Inside one parallel region each thread owns a static block [lo, hi) of the input: (1) count the digits of its block into a local histogram cnt[256]; barrier; (2) single turns all histograms into write offsets, ordered by digit first and thread second: off[t][d] = #keys with a smaller digit + #keys with digit d in threads 0..t-1; implicit barrier; (3) each thread scatters its block in input order to dst[off[t][d]++]. Every (digit, thread) pair owns a disjoint output range -> no races; thread order + in-order scan inside a block -> stable, which LSD radix sort needs. A barrier ends each pass (dst becomes the next src).

*Source:* lab extension (counting sort of Lab 8 / practice q30, made stable and multi-pass).

File: [`problems/p046_radix_sort.c`](problems/p046_radix_sort.c)

### P047 — Bitonic sort (sorting network), parallel compare-exchange stages

**Problem.** Sort an int array with the bitonic sorting network, parallelised with OpenMP. The network needs a power-of-two length; for other N pad with INT_MAX. Verify that the result is sorted and identical to qsort's output, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The network has log2(M)\*(log2(M)+1)/2 stages (k = 2, 4, ..., M; j = k/2, ..., 1). In stage (k, j) element i is paired with l = i ^ j; the pair is sorted ascending if (i & k) == 0, else descending. Each index is in exactly ONE pair per stage and only the smaller index (i < l) does the compare-exchange, so the iterations of a stage touch disjoint elements -> `#pragma omp parallel for` over i is race-free. The implicit barrier at the end of each parallel for separates the stages. Padding with INT_MAX puts the dummies at the end; the first N are the answer.

*Source:* lecture slides (sorting networks); lab extension of odd-even sort.

File: [`problems/p047_bitonic_sort.c`](problems/p047_bitonic_sort.c)

### P048 — Shell sort with the gap subsequences sorted in parallel

**Problem.** Implement Shell sort and parallelise it with OpenMP: for each gap, the gap independent subsequences (s, s+gap, s+2\*gap, ...), s = 0..gap-1, are insertion-sorted in parallel. Verify that the result is sorted and identical to qsort's output, report time and speedup for 1, 2, 4, 8 threads, and explain why the last pass (gap = 1) is serial.

**Solution.** For a fixed gap the subsequences partition the indices (index i belongs to subsequence i % gap), so insertion-sorting them touches disjoint elements -> `parallel for` over the start s is race-free. The implicit barrier after each gap's loop is needed: the next gap mixes elements of all subsequences. Two schedules are compared: schedule(dynamic) balances data-dependent insertion costs; schedule(static) gives each thread a range of NEIGHBOURING starts, so it owns contiguous ints in every row (no false sharing) - the subsequences have equal length, so static is also balanced. Gaps: Knuth's 1, 4, 13, 40, ... (h = 3h + 1).

*Source:* lab extension (Lab 9 sorting; insertion sort per independent subsequence).

File: [`problems/p048_shell_sort.c`](problems/p048_shell_sort.c)

### P049 — Parallel bucket sort of doubles uniform in [0, 1)

**Problem.** Sort N doubles drawn uniformly from [0, 1) with bucket sort: put x into bucket (int)(x \* B) of B buckets, then sort each bucket and concatenate. Parallelise both phases with OpenMP, verify that the result is sorted and identical to qsort's output, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Phase 1 (distribute) is a parallel counting sort on the bucket number: each thread counts the buckets of its static block into its own row cnt[t][0..B); barrier; single scans in (bucket, thread) order to get each thread's write offset inside each bucket and each bucket's start; implicit barrier; each thread scatters its block to its own disjoint ranges -> no races, no locks. Barrier. Phase 2: buckets are disjoint slices of out[], so `omp for schedule(dynamic)` over buckets, each sorted with qsort; dynamic because bucket sizes vary randomly.

*Source:* lab extension (Lab 9 sorting; distribution sort = scatter + independent sorts).

File: [`problems/p049_bucket_sort.c`](problems/p049_bucket_sort.c)

### P050 — k-th smallest element without sorting (quickselect, radix select, bisection)

**Problem.** Find the k-th smallest element (k = 1..N) of an unsorted int array without sorting it: (a) serial quickselect; (b) a parallel selection with OpenMP. Verify against a sorted copy for k = 1, N/2 and N, and report the time and speedup over quickselect for 1, 2, 4 and 8 threads.

**Solution.** (b) radix select: map each int to an order-preserving unsigned key (flip the sign bit) and fix the answer 8 bits at a time from the top. Each pass is a parallel histogram of the next 8-bit digit over the keys that still match the bits fixed so far (reduction(+:h[:256]), private copies -> race-free); walking the 256 counts tells which digit holds the k-th key and the new rank inside it. 4 passes, O(N) work, the input is only read. (c) bisection on the VALUE: the answer is the smallest v with count(a[i] <= v) >= k; each count is a parallel for with reduction(+), and binary search over [min, max] needs at most 32 counts.

*Source:* lab extension (order statistics; parallel histogram / counting).

File: [`problems/p050_kth_smallest.c`](problems/p050_kth_smallest.c)

### P051 — Parallel merge of two sorted arrays (co-rank / merge path)

**Problem.** Merge two sorted int arrays A (size m) and B (size n) into one sorted array C of size m + n, in parallel with OpenMP. Verify against the serial merge, including unequal sizes, empty inputs and many duplicates, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Split the OUTPUT into p equal ranges [k0, k1). For a split point k, the co-rank i = co_rank(k) is how many of the first k outputs come from A (the other j = k - i come from B). It is found by binary search on i: if A[i] <= B[j-1], A[i] belongs before B[j-1], so i is too small; else i is large enough (ties go to A, as in the serial merge -> stable). Thread t computes i0 = co_rank(k0), i1 = co_rank(k1) and serially merges A[i0..i1) with B[k0-i0 .. k1-i1) into C[k0..k1). The output ranges are disjoint and A, B are only read -> race-free, and every thread gets the same amount of output whatever the data (perfect load balance).

*Source:* lab extension (fixes the serial final merge of chunked sorts, practice q30).

File: [`problems/p051_parallel_merge.c`](problems/p051_parallel_merge.c)

### P052 — Merge sort with tasks where the MERGE is parallel too (Lab 9 extension)

**Problem.** Lab 9 sorted the two halves of merge sort as OpenMP tasks but merged them serially. Make the merge step parallel as well: recursive parallel merge (take the middle element of the larger input, binary-search its position in the other input, merge the two resulting pairs as tasks), with cutoffs for both the sort and the merge. Compare serial merge sort, the Lab 9 version (parallel recursion, serial merge) and the new version for 1, 2, 4, 8 threads, verify against qsort, and explain why the serial merge limited the speedup to about log n.

**Solution.** Sort: task for each half, taskwait, then merge a[0..h) and a[h..n) into tmp and copy back. Parallel merge of A (na) and B (nb) into C: let A be the larger, r = na/2, s = number of B elements < A[r] (lower bound); then A[r] lands at C[r+s], everything left of it is merge(A[0..r), B[0..s)) and everything right is merge(A[r+1..), B[s..)) -> two independent merges on disjoint input and output ranges (one as a task, one by the current thread, then taskwait). If B is larger, split B the same way using upper bound (A elements <= B[r] go first: ties from A first -> stable). The copy back is split into tasks too. Recursion stops at the cutoffs. Tasks are started from `parallel` + `single`; every task owns disjoint ranges of a and tmp -> race-free.

*Source:* Lab 9 Task 1 extension (session3/task1_mergesort.c).

File: [`problems/p052_mergesort_parallel_merge.c`](problems/p052_mergesort_parallel_merge.c)

### P053 — Sort every row (and every column) of a matrix in parallel

**Problem.** Given an R x C int matrix (row-major), (a) sort each row independently; (b) the same for a jagged matrix whose rows have different lengths; (c) sort each column independently. Parallelise with OpenMP, verify against the serial versions and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Rows (and columns) are disjoint, so sorting them is embarrassingly parallel: parallel for over rows, each iteration calls qsort on its own row -> no shared writes, no races. (a) equal rows -> schedule(static). (b) row cost ~ L log L varies with the length L -> schedule(dynamic) hands out rows on demand; static is shown for comparison. (c) a column is not contiguous, so each thread allocates ITS OWN buffer inside the parallel region (private), copies column j into it, sorts it and copies it back; a single shared buffer would be a race.

*Source:* lab extension (Lab 9 sorting; Lab 8 matrix loops).

File: [`problems/p053_sort_rows.c`](problems/p053_sort_rows.c)

### P054 — Search a value in a matrix: count, first occurrence, list of positions

**Problem.** Given an R x C int matrix (row-major) and a value v, find in parallel with OpenMP: (a) how many times v occurs; (b) the first occurrence in row-major order (or "none"); (c) the list of all (row, col) positions of v in row-major order. Verify against serial code for a value that occurs many times, exactly once and not at all, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** (a) parallel for over rows with reduction(+:count). (b) linear index r\*C + c is the row-major order, so the first occurrence is the MINIMUM matching index: reduction(min:first) with first = R\*C meaning "none"; inside a row the scan breaks at the first match. (c) ordered output = blocked count + scan: each thread takes a contiguous block of rows, pass 1 counts its matches, barrier, single turns the counts into offsets, implicit barrier, pass 2 writes its matches from its offset. Blocks are in row order, so the list is in row-major order; each thread writes its own slice -> race-free.

*Source:* lab extension (Lab 8 search task; reductions min/+, ordered output).

File: [`problems/p054_matrix_search.c`](problems/p054_matrix_search.c)

## Matrices and linear algebra

| # | Problem | File |
|---|---|---|
| P055 | Linear combination and Hadamard product of matrices | [`p055_matrix_linear_combo.c`](problems/p055_matrix_linear_combo.c) |
| P056 | Transposed matrix-vector product y = A^T x | [`p056_matvec_transpose.c`](problems/p056_matvec_transpose.c) |
| P057 | Outer product and rank-1 update | [`p057_outer_product.c`](problems/p057_outer_product.c) |
| P058 | Trace, anti-diagonal, border and all diagonal sums | [`p058_diagonal_sums.c`](problems/p058_diagonal_sums.c) |
| P059 | Matrix property checks with reduction(&&:) | [`p059_matrix_property_checks.c`](problems/p059_matrix_property_checks.c) |
| P060 | Sum of the strictly upper triangle: load balance and schedules | [`p060_triangle_sum.c`](problems/p060_triangle_sum.c) |
| P061 | Row normalisation and column standardisation | [`p061_normalize_rows_cols.c`](problems/p061_normalize_rows_cols.c) |
| P062 | Row with the largest sum, column with the smallest sum, row maxima | [`p062_row_col_extremes.c`](problems/p062_row_col_extremes.c) |
| P063 | Rotate a square matrix by 90 degrees (out of place and in place) | [`p063_rotate_matrix.c`](problems/p063_rotate_matrix.c) |
| P064 | Flip a matrix horizontally, vertically and rotate it by 180 degrees in place | [`p064_flip_matrix.c`](problems/p064_flip_matrix.c) |
| P065 | Rectangular matrix product C(m x n) = A(m x k) \* B(k x n) | [`p065_rect_matmul.c`](problems/p065_rect_matmul.c) |
| P066 | When is collapse(2) legal, and when is it correct? | [`p066_collapse_correctness.c`](problems/p066_collapse_correctness.c) |
| P067 | Matrix power A^p by repeated squaring | [`p067_matrix_power.c`](problems/p067_matrix_power.c) |
| P068 | Recursive (divide-and-conquer) matrix multiplication with tasks | [`p068_block_matmul_tasks.c`](problems/p068_block_matmul_tasks.c) |
| P069 | Product of two upper-triangular matrices (load balancing) | [`p069_triangular_matmul.c`](problems/p069_triangular_matmul.c) |
| P070 | LU decomposition without pivoting (Doolittle, in place) | [`p070_lu_decomposition.c`](problems/p070_lu_decomposition.c) |
| P071 | Solve Ax = b by Gaussian elimination with partial pivoting | [`p071_gaussian_elimination.c`](problems/p071_gaussian_elimination.c) |
| P072 | Determinant by Gaussian elimination | [`p072_determinant.c`](problems/p072_determinant.c) |
| P073 | Matrix inverse by Gauss-Jordan elimination (partial pivoting) | [`p073_matrix_inverse.c`](problems/p073_matrix_inverse.c) |
| P074 | Jacobi iteration for a linear system Ax = b | [`p074_jacobi_linear_solver.c`](problems/p074_jacobi_linear_solver.c) |
| P075 | Dominant eigenvalue by power iteration | [`p075_power_iteration.c`](problems/p075_power_iteration.c) |
| P076 | Sparse matrix-vector product (CSR) with very uneven rows | [`p076_sparse_csr_spmv.c`](problems/p076_sparse_csr_spmv.c) |

### P055 — Linear combination and Hadamard product of matrices

**Problem.** Given two N x N matrices A and B of doubles and two scalars alpha and beta, compute C = alpha\*A + beta\*B and the element-wise (Hadamard) product H = A o B, i.e. H[i][j] = A[i][j]\*B[i][j], serially and with OpenMP. Parallelise (1) only the outer (row) loop and (2) both loops with collapse(2). Verify every version and report time, speedup and efficiency for 1, 2, 4 and 8 threads.

**Solution.** Iteration (i, j) reads only A[i][j], B[i][j] and writes only its own C[i][j], so all N\*N iterations are independent: no race, no reduction. (1) parallel for on the i loop: each thread gets a block of whole rows; j is private because it is declared inside the loop. (2) collapse(2) merges the i and j loops into one N\*N iteration space that is split among the threads; legal because the loops are perfectly nested and rectangular. Matrices are flattened row-major: A[i][j] = A[i\*N + j] (i, j are long, no int overflow).

*Source:* lab extension of Lab 7 matrix addition (element-wise operations).

File: [`problems/p055_matrix_linear_combo.c`](problems/p055_matrix_linear_combo.c)

### P056 — Transposed matrix-vector product y = A^T x

**Problem.** Given an M x N matrix A and a vector x of length M, compute the length-N vector y = A^T x, i.e. y[j] = sum_i A[i][j]\*x[i], without forming A^T, serially and with OpenMP in three ways: (a) parallel over the output index j, each thread walking down column j of A; (b) parallel over the rows i with the array reduction reduction(+:y[:n]); (c) form T = A^T explicitly (in parallel) and then do an ordinary row-wise matvec y = T x. Verify all three, compare their times and explain the cache behaviour.

**Solution.** (a) y[j] is owned by one thread, which sums into a private s and writes y[j] once: race-free, but A is read with stride N (one useful double per cache line per row). (b) row i adds x[i]\*A[i][0..N-1] into ALL of y, so threads would race on y; reduction(+:y[:n]) gives each thread a private, zeroed copy of y and adds the copies into y at the end (OpenMP 4.5 array section reduction). A is read contiguously. (c) the transpose is parallel over the rows of A (each T[j][i] is written exactly once), then y[j] = dot(row j of T, x) is parallel over j with contiguous reads; it costs an extra pass over memory and M\*N more doubles. Matrices are flattened row-major: A[i][j] = A[i\*N + j], T[j][i] = T[j\*M + i].

*Source:* lab extension of Lab 7 matrix-vector product (Eijkhout Ch. 19 matvec).

File: [`problems/p056_matvec_transpose.c`](problems/p056_matvec_transpose.c)

### P057 — Outer product and rank-1 update

**Problem.** Given vectors x (length M) and y (length N), form the M x N outer product A = x y^T (A[i][j] = x[i]\*y[j]) and apply the rank-1 update B = B + alpha \* x y^T to an existing M x N matrix B, serially and with OpenMP using collapse(2). Verify both and report time, speedup and efficiency for 1, 2, 4 and 8 threads. Also show why collapse(2) matters when M is small.

**Solution.** Iteration (i, j) writes only its own element A[i][j] (or B[i][j]) and only reads x[i], y[j]: all M\*N iterations are independent, so there is no race and no reduction. collapse(2) turns the perfectly nested, rectangular i/j loops into one M\*N loop that is split evenly among the threads. With a tiny M (e.g. 3 rows) "parallel for" on i alone can keep only M threads busy, while collapse(2) uses them all. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (BLAS-2 operations dger / outer product).

File: [`problems/p057_outer_product.c`](problems/p057_outer_product.c)

### P058 — Trace, anti-diagonal, border and all diagonal sums

**Problem.** For an N x N integer matrix A compute, serially and with OpenMP: (1) the trace (sum of the main diagonal A[i][i]), the anti-diagonal sum (elements with i + j = N-1) and the sum of the border elements (first and last row and column, each corner counted once); (2) the sum of every one of the 2N-1 diagonals j - i = d, d = -(N-1)..N-1, stored in D[d + N-1], parallel over the diagonals. Verify against serial and report the timings.

**Solution.** (1) one parallel for over the rows with reduction(+:tr, an, bo): row i contributes A[i][i], A[i][N-1-i] and its border elements (the whole row if it is the first or last row, else its two end elements), so no element is counted twice. (2a) parallel for over the diagonal index k: each thread sums whole diagonals into a private s and writes D[k] once (race-free); diagonal lengths run 1, 2, .., N, .., 2, 1, so schedule(dynamic, 16) balances them. (2b) cache-friendly alternative: walk the matrix row by row and add A[i][j] into D[j - i + N-1]; rows share D, so use reduction(+:D[:2N-1]). Integer data with long sums, so results must match exactly. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (matrix index regions).

File: [`problems/p058_diagonal_sums.c`](problems/p058_diagonal_sums.c)

### P059 — Matrix property checks with reduction(&&:)

**Problem.** Write serial and OpenMP functions that test whether an N x N matrix A is (1) upper triangular (A[i][j] == 0 for all j < i), (2) lower triangular (A[i][j] == 0 for all j > i), (3) diagonal (A[i][j] == 0 for all i != j) and (4) strictly diagonally dominant (|A[i][i]| > sum_{j != i} |A[i][j]| for every row i). Test them on matrices that have each property and on matrices that fail it by a single element, verify the parallel answers against serial and the expected answers, and time each check.

**Solution.** Each check is "AND over many independent conditions", so reduction(&&:ok): every thread starts with a private ok = 1, clears it when it finds a violation, and the copies are AND-ed at the end (no race on ok). (1)/(2) parallel for over rows i; the inner j loop covers only the triangle, so rows have unequal work -> schedule(dynamic, 32) (collapse is not allowed: the j bound depends on i). (3) all (i, j) pairs: rectangular, so collapse(2). (4) parallel over rows; the off-diagonal row sum is a private local of the iteration. A parallel loop cannot "break" early, so a failing matrix is still scanned to the end by the parallel version. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (MATRIX_RULES.md section 4: special matrices).

File: [`problems/p059_matrix_property_checks.c`](problems/p059_matrix_property_checks.c)

### P060 — Sum of the strictly upper triangle: load balance and schedules

**Problem.** Compute S = sum of A[i][j] over the strictly upper triangle (j > i) of an N x N integer matrix, serially and with OpenMP parallel over the rows. Row i has only N-1-i elements, so the work per iteration shrinks with i. Compare schedule(static), schedule(static, 1), schedule(dynamic, 16) and schedule(guided), and show the "row pairing" trick (pair row i with row N-1-i) that makes a plain static schedule balanced. Verify every version and report the timings and the work done by each thread.

**Solution.** All versions use reduction(+:s) on a long sum (integer data: exact). static gives thread 0 the first N/T rows, which hold the longest rows (with T = 4 it gets 7/16 of the work instead of 1/4); (static, 1) deals rows round robin; dynamic hands out 16 rows at a time to whichever thread is free; guided starts with big chunks that shrink. Row pairing: iteration p does rows p and N-1-p, which together hold (N-1-p) + p = N-1 elements, so every iteration has the same work and static is balanced (for odd N the middle row is paired with itself and counted once). Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (Eijkhout Ch. 19 loop schedules, triangular loops).

File: [`problems/p060_triangle_sum.c`](problems/p060_triangle_sum.c)

### P061 — Row normalisation and column standardisation

**Problem.** Given an M x N matrix A of positive doubles, (1) row-normalise it: divide every row by its sum so each row sums to 1; (2) column-standardise it: replace A[i][j] by (A[i][j] - mean_j) / sd_j, where mean_j and sd_j are the mean and (population) standard deviation of column j, so each column has mean 0 and standard deviation 1. Do both serially and with OpenMP: rows parallel over rows; columns (a) parallel over columns and (b) with column sums computed by an array reduction over rows. Verify and time all versions.

**Solution.** (1) row i is read and written only by the thread that owns iteration i; its sum is a private local, so parallel for over i is race-free and reads contiguously. (2a) parallel for over j: column j (its mean, sd and update) is owned by one thread, race-free, but walks memory with stride N. (2b) three row-parallel passes: column sums with reduction(+:sum[:n]), squared deviations with reduction(+:sq[:n]) (rows share the per-column totals, hence the array reduction), then an element-wise update parallel over rows; all reads are contiguous. Two-pass variance (mean first) avoids the cancellation of E[x^2] - mean^2. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (data preprocessing: normalise / standardise).

File: [`problems/p061_normalize_rows_cols.c`](problems/p061_normalize_rows_cols.c)

### P062 — Row with the largest sum, column with the smallest sum, row maxima

**Problem.** For an N x N integer matrix A find, serially and with OpenMP, (1) the row with the maximum row sum, (2) the column with the minimum column sum (ties broken in favour of the smaller index in both cases) and (3) the maximum element of every row, rmax[i] = max_j A[i][j]. Verify against serial (including deliberately tied rows/columns) and report the timings.

**Solution.** (1a) per-thread best + critical: each thread scans its rows with a private (sum, index) best, then merges it into the shared best once inside "#pragma omp critical". (1b) two phases: all row sums R[i] in parallel (each thread writes its own R[i]), then an argmax over R with a user-defined reduction (#pragma omp declare reduction, OpenMP 4.0). (2) column sums with reduction(+:C[:n]) over rows (contiguous reads; rows share C), then an argmin with per-thread best + critical. Every merge uses "better value, or equal value and smaller index", so the answer does not depend on the schedule. (3) parallel for over rows; each thread writes its own rmax[i]. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (Lab 7 Task 3 max/min with index, applied to a matrix).

File: [`problems/p062_row_col_extremes.c`](problems/p062_row_col_extremes.c)

### P063 — Rotate a square matrix by 90 degrees (out of place and in place)

**Problem.** Rotate an N x N integer matrix A by 90 degrees clockwise and anticlockwise, serially and with OpenMP: (1) out of place into a new matrix B (clockwise: B[j][N-1-i] = A[i][j]; anticlockwise: B[N-1-j][i] = A[i][j]) and (2) in place, without a second matrix (clockwise = transpose in place, then reverse every row; anticlockwise = transpose in place, then reverse the order of the rows). Verify the versions against each other, show 3x3 and 4x4 examples and report the timings.

**Solution.** (1) a rotation maps every (i, j) to a different (row, col) of B, so a parallel for over i writes distinct elements: no race. (2) one parallel region, two omp for loops. Phase 1, transpose: iteration i swaps A[i][j] with A[j][i] for j > i only, so each pair is swapped exactly once and by one thread (rows have unequal work -> schedule(dynamic, 16)). Phase 2 (reverse each row, or swap row i with row N-1-i for i < N/2) touches disjoint pairs. The implicit barrier at the end of the first omp for guarantees phase 2 starts only after the whole transpose is finished. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (classic interview problem "rotate image").

File: [`problems/p063_rotate_matrix.c`](problems/p063_rotate_matrix.c)

### P064 — Flip a matrix horizontally, vertically and rotate it by 180 degrees in place

**Problem.** For an M x N integer matrix A, write serial and OpenMP functions that, in place, (1) flip it horizontally (reverse every row), (2) flip it vertically (swap row i with row M-1-i) and (3) rotate it by 180 degrees (A[i][j] <-> A[M-1-i][N-1-j]). Parallelise only over the first half where each swap pairs two elements, and explain why that is race-free. Verify the results and report the timings.

**Solution.** An in-place flip is a set of disjoint swaps. Looping over the first half only makes each pair belong to exactly one iteration, so no two threads ever touch the same element. (1) parallel for over rows i, inner j < N/2 swaps A[i][j] with A[i][N-1-j] (each row belongs to one thread). (2) parallel for over i < M/2, swapping whole rows i and M-1-i. (3) the 180-degree rotation reverses the flattened array: parallel for over k < M\*N/2 swapping A[k] with A[M\*N-1-k]. For odd sizes the middle row/column/element stays in place. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (image flips: mirror / upside down).

File: [`problems/p064_flip_matrix.c`](problems/p064_flip_matrix.c)

### P065 — Rectangular matrix product C(m x n) = A(m x k) \* B(k x n)

**Problem.** Multiply a rectangular m x k matrix A by a k x n matrix B, with m, k and n all different (e.g. 600 x 800 times 800 x 700), serially and with OpenMP: (1) the ijk loop order with collapse(2) on the (i, j) loops and (2) the ikj (row-wise) order, parallel over i. Check the dimension rule in code (columns of A must equal rows of B) and print an error otherwise. Verify both versions and report the timings.

**Solution.** C[i][j] = sum_p A[i][p]\*B[p][j] (row i of A times column j of B), so every (i, j) is computed by one iteration that owns C[i][j]. (1) collapse(2) merges the rectangular, perfectly nested i and j loops (m\*n iterations); the dot-product sum s is private (declared inside). The p loop must NOT be parallelised/collapsed: all p add into the same C[i][j]. (2) ikj: for each i and p, C[i][:] += A[i][p]\*B[p][:] streams rows of B and C contiguously; row i of C belongs to one thread, so parallel for over i is race-free. Matrices are flattened row-major: A[i][p] = A[i\*k + p], B[p][j] = B[p\*n + j], C[i][j] = C[i\*n + j].

*Source:* lab extension of Lab 7 Task 5 (matrix multiplication) to rectangular sizes.

File: [`problems/p065_rect_matmul.c`](problems/p065_rect_matmul.c)

### P066 — When is collapse(2) legal, and when is it correct?

**Problem.** For each loop nest decide whether collapse can be used, demonstrate what goes wrong otherwise, and give a correct parallel version: (a) matvec with y[i] = 0 inside the i loop, before the j loop: for i { y[i] = 0; for j y[i] += A[i][j]\*x[j]; }   with collapse(2); (b) the same with the initialisation moved to a separate loop, so the nest for i for j y[i] += A[i][j]\*x[j] is perfectly nested: collapse(2) compiles, but is the result right? (c) a triple loop for i for j for k X[i][j] += f(i, j, k): may we use collapse(2) on (i, j)? collapse(3)? Print the explanations, show the wrong results, and verify and time the correct versions.

**Solution.** Collapse(n) needs (legality) perfectly nested, rectangular loops (OpenMP 4.5) and (correctness) no two iterations of the collapsed space writing the same location. (a) is not perfectly nested: gcc 9 and 12 reject it ("not enough perfectly nested loops", checked); gcc 14+ accept it as OpenMP 5.1 "intervening code" but may run y[i] = 0 on every collapsed iteration, so the result is wrong anyway (compile with -DDEMO_IMPERFECT to try it). (b) is legal but RACES: iterations (i, j1) and (i, j2) may run on different threads and both update y[i]. Fixes: parallelise only i (private sum per row), or keep collapse(2) and add reduction(+:y[:n]) (or an atomic, slow). (c) collapse(2) is correct: each (i, j) owns X[i][j] and runs its k loop serially; collapse(3) races because different k for the same (i, j) go to different threads. The race demos use schedule(static, 1) so that neighbouring iterations really run at the same time; integer-valued data makes every correct sum exact, so lost updates are counted exactly. Matrices are flattened row-major: A[i][j] = A[i\*N + j], X[i][j] = X[i\*NJ + j].

*Source:* textbook (Eijkhout, Parallel Programming) Ex 19.11, 19.14, 19.15, 19.16.

File: [`problems/p066_collapse_correctness.c`](problems/p066_collapse_correctness.c)

### P067 — Matrix power A^p by repeated squaring

**Problem.** Compute A^p for an N x N matrix A and an integer p >= 0 using repeated squaring (O(log p) matrix multiplications instead of p-1), serially and with OpenMP. To keep the numbers bounded, work with integers modulo the prime P = 1000003. Verify the parallel result against serial, against naive repeated multiplication (small p) and on a 2x2 Fibonacci example, and report the timings.

**Solution.** Binary exponentiation: R = I, B = A; for each bit of p (lowest first): if the bit is 1, R = R\*B; then B = B\*B. This exponent loop is SEQUENTIAL: each step needs the previous R and B. The parallelism is inside each matrix multiply: C = X\*Y (mod P) with parallel for over the rows i of C (ikj order: row i of C += X[i][k] \* row k of Y). Row i of C is written by one thread and X, Y are only read, so there is no race. The product goes to a separate buffer (never C = C\*Y in place). Values are < P < 2^20, so a product is < 2^40 and a row sum of N products fits in 64 bits for N < 9e6; reduce mod P once per element. long is 64-bit on Linux/macOS (LP64). Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (fast exponentiation; Fibonacci / graph path counting).

File: [`problems/p067_matrix_power.c`](problems/p067_matrix_power.c)

### P068 — Recursive (divide-and-conquer) matrix multiplication with tasks

**Problem.** Multiply two N x N matrices, C = A\*B, by recursive divide and conquer: split A, B and C into 2 x 2 blocks (quadrants) and use C11 = A11 B11 + A12 B21     C12 = A11 B12 + A12 B22 C21 = A21 B11 + A22 B21     C22 = A21 B12 + A22 B22, computing the four quadrants of C as independent OpenMP tasks, and switch to a serial ikj kernel below a cutoff block size. Handle any N. Verify against the serial product and report the timings.

**Solution.** The four quadrants of C are disjoint, so the four tasks never write the same element. Inside one task the two products for the same quadrant (e.g. A11 B11 then A12 B21, both adding into C11) run one after the other, so there is no race on C11. Every call ends with "#pragma omp taskwait", so when a recursive call returns, all of its sub-tasks have finished; the next call into the same quadrant starts only then. The top call is made by one thread (parallel + single); the other threads execute the tasks. General N: halves are m/2 and m - m/2 (may differ by one), so no padding is needed; the blocks are views into the full matrices with row stride ld. Matrices are flattened row-major: A[i][j] = A[i\*N + j]; a block starting at P with row stride ld has element (i, j) at P[i\*ld + j].

*Source:* lab extension (task parallelism, recursive algorithms; cf. Strassen).

File: [`problems/p068_block_matmul_tasks.c`](problems/p068_block_matmul_tasks.c)

### P069 — Product of two upper-triangular matrices (load balancing)

**Problem.** A and B are N x N upper-triangular matrices (zero below the diagonal). Their product C = A\*B is upper triangular, with C[i][j] = sum_{k=i..j} A[i][k]\*B[k][j] for j >= i and C[i][j] = 0 for j < i. Compute it serially and with OpenMP using only these terms, compare schedule(static) with schedule(dynamic) (and static,1), verify against an ordinary full matrix multiplication and report the timings.

**Solution.** Row-wise (ikj) form: for k = i..N-1, row i of C (columns j = k..N-1) += A[i][k] \* row k of B (only j >= k is nonzero). Row i of C is written only by iteration i, so parallel for over i is race-free. Row i costs (N-i)(N-i+1)/2 multiply-adds: the first rows are far more expensive than the last, so schedule(static), which gives each thread one contiguous block of rows, is badly unbalanced (with 4 threads, thread 0 gets ~58% of the work). schedule(dynamic, 4) hands out small chunks to whichever thread is free; schedule(static, 1) deals the rows round robin. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (Eijkhout Ch. 19 schedules; triangular loops).

File: [`problems/p069_triangular_matmul.c`](problems/p069_triangular_matmul.c)

### P070 — LU decomposition without pivoting (Doolittle, in place)

**Problem.** Factor an N x N strictly diagonally dominant matrix A as A = L\*U, with L unit lower triangular and U upper triangular, in place (U on and above the diagonal, the multipliers of L below it), without pivoting, serially and with OpenMP. Explain which loops can run in parallel, verify that L\*U reproduces A and that the parallel factors match serial, and report timings.

**Solution.** For k = 0..N-2: for every row i > k, l = A[i][k] / A[k][k] is stored in A[i][k] and row i is updated: A[i][j] -= l \* A[k][j] for j > k. The k loop is SEQUENTIAL: step k uses the values produced by step k-1. For a fixed k the rows i > k are independent: each reads the pivot row k (not changed in this step) and writes only its own row. (a) one parallel region around the k loop; every thread runs the k loop and "#pragma omp for" shares the rows i; its implicit barrier finishes step k before step k+1 begins (k is private: declared in the loop). (b) the same with two omp for loops per step: first the multipliers (column k), then the trailing update over (i, j) with collapse(2) (rectangular for fixed k). Diagonal dominance guarantees nonzero pivots, so no pivoting is needed. Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* textbook (Eijkhout, Parallel Programming) Ex 19.10.

File: [`problems/p070_lu_decomposition.c`](problems/p070_lu_decomposition.c)

### P071 — Solve Ax = b by Gaussian elimination with partial pivoting

**Problem.** Solve the N x N linear system A x = b by Gaussian elimination with partial pivoting followed by back substitution, serially and with OpenMP. For each column k: find the pivot row (largest |A[i][k]|, i >= k), swap it into row k, and eliminate column k from the rows below. Verify that the residual ||Ax - b|| is tiny, that x matches the serial solution and the known true solution, and report the timings.

**Solution.** The k loop is sequential (step k works on the matrix left by step k-1). Inside step k: (1) pivot search = parallel argmax of |A[i][k]|: each thread keeps a private (value, row) best, merged in a critical section (ties -> smaller row, so the choice equals serial); (2) the row swap is O(N), done serially; (3) elimination: rows i > k are independent (each reads pivot row k, writes only row i and b[i]) -> parallel for over i. Back substitution: x[i] = (b[i] - sum_{j>i} A[i][j] x[j]) / A[i][i] runs i = N-1 down to 0 sequentially (x[i] needs all later x[j]); the inner dot product is a reduction(+:s). Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (Eijkhout Ch. 19 exercises on Gaussian elimination).

File: [`problems/p071_gaussian_elimination.c`](problems/p071_gaussian_elimination.c)

### P072 — Determinant by Gaussian elimination

**Problem.** Compute the determinant of an N x N matrix by Gaussian elimination with partial pivoting: det(A) = (product of the pivots) \* (-1)^(number of row swaps). Do it serially and with OpenMP (parallel elimination as in P071). Test on matrices with known determinants: small 3x3 matrices worked by hand, a triangular matrix (det = product of the diagonal), the same with its rows reversed, a permutation matrix, a matrix with two equal rows (det = 0) and det(2A) = 2^N det(A). Verify parallel against serial and against the known values, and time it.

**Solution.** The k loop is sequential; in step k the pivot search is a parallel argmax (private (value, row) per thread + critical, ties -> smaller row), the swap is serial and flips the sign, and the rows below are eliminated with parallel for over i (each row written by one thread, pivot row only read). Row operations "row_i -= l \* row_k" do not change the determinant, so det is the product of the diagonal of the final upper-triangular matrix times the swap sign. The product of N pivots overflows a double for large N (|det| of a random 1000 x 1000 matrix is ~1e1000), so the result is kept as sign \* exp(logabs) with logabs = sum of log|pivot|; a zero pivot column means det = 0 (sign 0). Matrices are flattened row-major: A[i][j] = A[i\*N + j].

*Source:* lab extension (MATRIX_RULES.md section 3: determinant rules).

File: [`problems/p072_determinant.c`](problems/p072_determinant.c)

### P073 — Matrix inverse by Gauss-Jordan elimination (partial pivoting)

**Problem.** Given a nonsingular N x N matrix A of doubles (flattened row-major, A[(long)i\*N + j]), compute inv(A) by Gauss-Jordan elimination on the augmented matrix [A | I] with partial pivoting, serially and with OpenMP. Verify that A \* inv(A) is (close to) the identity and that the parallel inverse matches the serial one, and report the time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The pivot columns k = 0..N-1 are processed in order (step k+1 needs the matrix left by step k), so the k loop stays serial. In step k the pivot row (largest |M[i][k]|, i >= k) is swapped into row k and divided by the pivot; this is O(N) work and stays serial. Then every other row i does row_i -= M[i][k] \* row_k. These N-1 row updates are independent: a thread writes only its own rows i and only reads row k, which nobody writes during the update, so "#pragma omp parallel for" over i is race-free. All rows do the same work, so schedule(static) is fine. The augmented matrix M is N x 2N, row-major M[(long)i\*2N + j]; its right half ends up as inv(A).

*Source:* lab extension (linear algebra kernels).

File: [`problems/p073_matrix_inverse.c`](problems/p073_matrix_inverse.c)

### P074 — Jacobi iteration for a linear system Ax = b

**Problem.** Solve Ax = b, A a dense N x N strictly diagonally dominant matrix (flattened row-major, A[(long)i\*N + j]), with the Jacobi method: start from x = 0 and repeat x_new[i] = (b[i] - sum_{j != i} a_ij \* x[j]) / a_ii until max_i |x_new[i] - x[i]| < tol (or a maximum number of iterations). Parallelise with OpenMP, verify against the serial solver (same iteration count, same x) and check the residual b - Ax.

**Solution.** In one iteration every x_new[i] reads only the OLD vector x, so the N rows are independent and "parallel for" over i is race-free, provided x and x_new are different arrays (double buffering: swap the two pointers after each iteration). The convergence test needs the largest change over all i: reduction(max:diff). Max is exact in any order and every x_new[i] is computed by the same code as in the serial solver, so the parallel run takes exactly the same number of iterations and gives the same x. (Strict diagonal dominance, |a_ii| > sum_{j != i} |a_ij|, guarantees that Jacobi converges.)

*Source:* lab extension (iterative solvers).

File: [`problems/p074_jacobi_linear_solver.c`](problems/p074_jacobi_linear_solver.c)

### P075 — Dominant eigenvalue by power iteration

**Problem.** For a symmetric N x N matrix A (flattened row-major, A[(long)i\*N + j]) find the eigenvalue of largest magnitude by power iteration: start with a unit vector x and repeat y = A x, lambda = x . y (Rayleigh quotient, |x| = 1), x = y / |y|, until lambda changes by less than tol (relative). Parallelise with OpenMP and verify against the serial version and against a matrix whose eigenvalues are known.

**Solution.** Each step has three parallel loops: (1) y = A x, parallel over rows (row i writes only y[i]); (2) x . y and y . y with reduction(+:xy, yy); (3) x = y / |y|, parallel over i. Between them there must be a barrier (all of y before the dot products, the full |y| before normalising), and the steps themselves are sequential. Version A: one "parallel for" per loop (3 fork/joins per step) - simplest. Version B: ONE parallel region around the whole iteration; the loops are "omp for" (implicit barriers) and a "single" does the scalar work (lambda, 1/|y|, convergence flag, resetting the reduction variables). All threads read the shared flag "done" after a barrier, so they all leave the while loop in the same step. B saves the fork/join per loop, which only matters when N is small; for large N the O(N^2) matvec dominates and A = B. Test matrix: A = Q D Q with Q = I - 2uu^T (|u| = 1, a Householder reflection), so A is symmetric with eigenvalues exactly D = diag(d): d[0] = 8 (dominant), the others in [1, 5]; the dominant eigenvector is Q e0.

*Source:* lab extension (numerical linear algebra).

File: [`problems/p075_power_iteration.c`](problems/p075_power_iteration.c)

### P076 — Sparse matrix-vector product (CSR) with very uneven rows

**Problem.** Multiply an N x N sparse matrix stored in CSR format by a vector, y = A x. CSR: row_ptr[N+1], col_idx[nnz], val[nnz]; the nonzeros of row i are val[k], at column col_idx[k], for k = row_ptr[i] .. row_ptr[i+1]-1. The rows have very different lengths: in the first quarter of the matrix every 100th row is 1000x longer than a normal row. Build the CSR matrix serially from a deterministic generator, parallelise the product with OpenMP and compare schedule(static), schedule(dynamic, c) and schedule(guided). Verify against the serial product (and against a dense product for a small matrix).

**Solution.** Y[i] depends only on row i, and only one thread writes y[i], so "parallel for" over rows is race-free. The work of row i is its length row_ptr[i+1] - row_ptr[i], which varies by 1000x and is concentrated at the start of the matrix: schedule(static) gives thread 0 a contiguous block that holds almost all the long rows (load imbalance). dynamic hands out chunks of c rows on demand, so threads that get light rows simply take more chunks. guided hands out big chunks first (about remaining/threads) and smaller later; here the heavy rows are at the start, so its first big chunk is also unbalanced. Each row is summed in the same order as the serial code, so the results are bitwise equal.

*Source:* lab extension (loop scheduling).

File: [`problems/p076_sparse_csr_spmv.c`](problems/p076_sparse_csr_spmv.c)

## Grids, stencils and images

| # | Problem | File |
|---|---|---|
| P077 | Equation solver - red-black Gauss-Seidel vs Jacobi | [`p077_gauss_seidel_red_black.c`](problems/p077_gauss_seidel_red_black.c) |
| P078 | Wavefront (anti-diagonal) parallelisation | [`p078_wavefront_antidiagonal.c`](problems/p078_wavefront_antidiagonal.c) |
| P079 | 2D heat equation, explicit time stepping | [`p079_heat_explicit.c`](problems/p079_heat_explicit.c) |
| P080 | Point-wise image operations (grayscale, negative, threshold, brightness/contrast) | [`p080_image_pointwise.c`](problems/p080_image_pointwise.c) |
| P081 | Histogram equalisation of an 8-bit grayscale image | [`p081_histogram_equalization.c`](problems/p081_histogram_equalization.c) |
| P082 | 3x3 median filter (salt-and-pepper noise removal) | [`p082_median_filter.c`](problems/p082_median_filter.c) |
| P083 | CNN pooling layers (max and average pooling, k x k window, stride s) | [`p083_pooling.c`](problems/p083_pooling.c) |
| P084 | Summed-area table (2D prefix sum, integral image) and rectangle queries | [`p084_summed_area_table.c`](problems/p084_summed_area_table.c) |
| P085 | Cellular automata (1D elementary rules, 2D majority vote) | [`p085_cellular_automaton.c`](problems/p085_cellular_automaton.c) |
| P086 | Mandelbrot set - escape-time image and area by random sampling | [`p086_mandelbrot.c`](problems/p086_mandelbrot.c) |

### P077 — Equation solver - red-black Gauss-Seidel vs Jacobi

**Problem.** (Lecture slides, equation solver kernel.) An (N+2) x (N+2) grid A (flattened row-major, A[(long)i\*(N+2) + j]) has fixed boundary values; the N x N interior points are updated in place, A[i][j] = 0.2 \* (A[i][j] + A[i-1][j] + A[i+1][j] + A[i][j-1] + A[i][j+1]), sweep after sweep, until the average change per point, diff / (N\*N) with diff = sum |A_new - A_old|, is below tol. The in-place sequential sweep uses the NEW values of the up and left neighbours, so its iterations are not independent. Parallelise it with red-black ordering, compare with Jacobi (double buffer), verify that the parallel red-black run equals the serial red-black run exactly, and show that all methods converge.

**Solution.** Colour point (i,j) red if (i+j) is even and black if odd. The four neighbours of a red point are black and vice versa. A sweep updates ALL red points (in parallel: they only read black points, which nobody writes in this half-sweep), then, after a barrier, all black points (they read the new red values). Each colour half-sweep is "omp for reduction(+:diff)" over rows inside one parallel region; the implicit barrier at the end of the red loop is exactly the barrier we need. Every point gets the same value in any execution order, so parallel red-black == serial red-black bit for bit. Red-black is a different ORDER from the row-by-row sweep, so its values differ from the sequential Gauss-Seidel values, but it solves the same equations at the same rate (equally valid within tol). Jacobi reads only the old grid A and writes a second grid B (then swaps): trivially parallel, but it needs more sweeps (about 1.5-2x here). Part 1 runs every method to convergence on a small grid (Nc = 100, tol = 1e-5). Part 2 times a big grid (N = 1000) for a fixed number of sweeps (tol = 0, so the sweep cap stops it): converging a 1000 x 1000 grid takes about 100x more sweeps (sweeps grow like N^2). Boundary: top row 100, other edges 0; interior starts at 25 (not 0, to avoid slow subnormal numbers).

*Source:* lecture slides (equation solver); lab extension.

File: [`problems/p077_gauss_seidel_red_black.c`](problems/p077_gauss_seidel_red_black.c)

### P078 — Wavefront (anti-diagonal) parallelisation

**Problem.** (Lecture slides; textbook Ex 24.2.) On an N x N grid (flattened row-major, X[(long)i\*N + j]) with row 0 and column 0 given, compute x[i][j] = x[i-1][j] + x[i][j-1]     for i, j = 1..N-1 (here mod 1000000007 so the numbers stay bounded; with all 1s on the border x[i][j] = C(i+j, i), Pascal's triangle). Also do K in-place Gauss-Seidel sweeps A[i][j] = 0.2\*(A[i][j] + up + down + left + right). Neither loop nest can be parallelised over rows or over columns. Find the parallelism, implement it with OpenMP and verify EXACT equality with the serial row-by-row loops.

**Solution.** (i,j) needs (i-1,j) and (i,j-1), which lie on the previous anti-diagonal d-1 (d = i+j). All points on one anti-diagonal are independent. So loop over d sequentially and run the points of diagonal d in parallel ("omp for" over i, with j = d - i), inside one parallel region; the implicit barrier after each diagonal makes diagonal d complete before d+1 starts. Every point is computed from exactly the same inputs as in the serial loop, so the result is identical. For the in-place Gauss-Seidel sweep the same works: up/left lie on diagonal d-1 (already new this sweep) and down/right on d+1 (still old), exactly as in the row-by-row order. Load imbalance: diagonal lengths grow 1, 2, ..., N-1 and shrink again, so near the corners there are fewer points than threads, and each diagonal costs a barrier (2N barriers). Consecutive points on a diagonal are N-1 elements apart in memory (poor cache use). Blocked wavefront: cut the grid into B x B tiles and run the tiles on each tile anti-diagonal in parallel; inside a tile go row by row (cache-friendly, only 2N/B barriers).

*Source:* lecture slides (wavefront); textbook Ex 24.2.

File: [`problems/p078_wavefront_antidiagonal.c`](problems/p078_wavefront_antidiagonal.c)

### P079 — 2D heat equation, explicit time stepping

**Problem.** Simulate heat diffusion on an N x N plate (flattened row-major, u[(long)i\*N + j]) for T time steps with the explicit scheme u_new[i][j] = u[i][j] + r \* (u[i-1][j] + u[i+1][j] + u[i][j-1] + u[i][j+1] - 4 u[i][j]) for the interior points, with fixed (Dirichlet) boundary values: top edge 100, other edges 0, interior initially 25. Use r = 0.2 (stable for r <= 0.25). Parallelise with OpenMP, verify against the serial code, and compare a "parallel for" per time step with one parallel region around the whole time loop.

**Solution.** Double buffering: step t reads only u and writes only u_new, so all points of a step are independent ("for" over rows is race-free); then the two pointers are swapped. Both buffers hold the boundary values, which are never written. Steps are sequential: step t+1 needs all of step t. Version A: "#pragma omp parallel for" inside the time loop (one fork/join per step; the master swaps the pointers between regions). Version B: one "#pragma omp parallel" around the time loop; each step is an "omp for" whose implicit barrier ends the step, and every thread swaps its OWN private copies of the two pointers (firstprivate) in the same way, so no extra barrier or single is needed for the swap. Each point uses the same expression as the serial code: results are equal bit for bit.

*Source:* lab extension (stencil codes).

File: [`problems/p079_heat_explicit.c`](problems/p079_heat_explicit.c)

### P080 — Point-wise image operations (grayscale, negative, threshold, brightness/contrast)

**Problem.** A W x H colour image is stored as interleaved 8-bit RGB, row-major: pixel (y, x) is rgb[3\*((long)y\*W + x) + c], c = 0 (R), 1 (G), 2 (B). Compute (1) the grayscale image g = 0.299 R + 0.587 G + 0.114 B (rounded), stored row-major g[(long)y\*W + x]; from g compute (2) the negative 255 - g, (3) the binary image 255 if g >= threshold else 0, and (4) brightness and contrast alpha\*g + beta clamped to [0, 255]. Parallelise each operation with OpenMP, verify against the serial code and report the speedups.

**Solution.** Every output pixel depends only on the same input pixel, so all pixels are independent: "#pragma omp parallel for collapse(2)" over (y, x) is race-free (each iteration writes its own output pixel). collapse(2) merges the two loops into one W\*H iteration space, which balances well even when H is small. The serial and parallel versions call the same per-pixel helper, so the results are identical (exact check). Variant (2') uses "parallel for simd collapse(2)" so each thread's chunk is also vectorised (see Notes).

*Source:* lab extension (image processing).

File: [`problems/p080_image_pointwise.c`](problems/p080_image_pointwise.c)

### P081 — Histogram equalisation of an 8-bit grayscale image

**Problem.** A W x H grayscale image (unsigned char, row-major img[(long)y\*W + x]) has low contrast. Equalise it: (1) histogram h[v] = number of pixels with value v (v = 0..255); (2) cumulative histogram cdf[v] = h[0] + ... + h[v]; (3) lookup table lut[v] = round((cdf[v] - cdf_min) \* 255 / (N - cdf_min)), where N = W\*H and cdf_min = cdf of the darkest value present; (4) remap out[p] = lut[img[p]]. Parallelise with OpenMP, verify against the serial code exactly, and print a histogram summary before and after.

**Solution.** (1) many pixels hit the same bin, so h[img[p]]++ in a plain parallel for is a race. Version R: reduction(+:h[:256]) (array-section reduction, OpenMP 4.5): each thread gets a private zeroed copy of h, and the copies are added at the end. Version P (works with any OpenMP): a private local histogram per thread, then each thread adds it into h inside a critical section (256 adds per thread). Counting is integer, so both are exact. (2)-(3) are only 256 steps and the prefix sum is sequential: keep them serial. (4) each output pixel depends only on its input pixel and the read-only lut: "parallel for" is race-free.

*Source:* lab extension (image processing).

File: [`problems/p081_histogram_equalization.c`](problems/p081_histogram_equalization.c)

### P082 — 3x3 median filter (salt-and-pepper noise removal)

**Problem.** Apply a 3x3 median filter to a W x H 8-bit grayscale image (row-major img[(long)y\*W + x]): out(y,x) = median of the 9 pixels around (y,x). At the border, replicate the edge pixels (clamp coordinates into the image), so the output has the same size. Parallelise with OpenMP, verify against the serial filter, and show that salt-and-pepper noise (random pixels set to 0 or 255) is removed on a tiny example.

**Solution.** Each output pixel is computed from the INPUT image only and written to a separate output image, so all pixels are independent: "#pragma omp parallel for collapse(2)" over (y, x) is race-free (writing into the input image in place would be wrong: neighbours would see already-filtered values). Per pixel: gather the 9 values into a private local array, insertion-sort it, take element 4 (the middle). Same code in serial and parallel -> identical output.

*Source:* lab extension (image processing).

File: [`problems/p082_median_filter.c`](problems/p082_median_filter.c)

### P083 — CNN pooling layers (max and average pooling, k x k window, stride s)

**Problem.** Given an H x W feature map of doubles (row-major in[(long)i\*W + j]), compute k x k pooling with stride s (no padding): the output has Ho = (H - k)/s + 1 rows and Wo = (W - k)/s + 1 columns (H/2 x W/2 for k = s = 2), and out[oi][oj] is the max (max pooling) or the mean (average pooling) of the window in[oi\*s .. oi\*s+k-1][oj\*s .. oj\*s+k-1]. Parallelise with OpenMP, verify against the serial code, and print a 4 x 4 -> 2 x 2 example.

**Solution.** Each output cell reads its own window of the input and writes only out[oi][oj], so the output cells are independent: "#pragma omp parallel for collapse(2)" over (oi, oj) is race-free (overlapping windows when s < k are fine: the input is only read). The window loop inside a cell is the same in the serial and parallel code, so the results are identical, even for the average (same additions in the same order).

*Source:* lab extension (deep-learning kernels).

File: [`problems/p083_pooling.c`](problems/p083_pooling.c)

### P084 — Summed-area table (2D prefix sum, integral image) and rectangle queries

**Problem.** Given an H x W integer matrix A (row-major A[(long)i\*W + j]), compute S[i][j] = sum of A[0..i][0..j]. Then answer Q queries "sum of A over the rectangle rows r1..r2, columns c1..c2" in O(1) each: sum = S[r2][c2] - S[r1-1][c2] - S[r2][c1-1] + S[r1-1][c1-1] (terms with index -1 are 0). Parallelise building S and answering the queries with OpenMP; verify S against the serial code and some query answers against brute-force sums.

**Solution.** Build S in two passes. Pass 1: prefix sum along each row; rows are independent, so "parallel for" over rows (each thread runs whole rows, sequentially along the row). Pass 2: prefix sum down each column of the pass-1 result; columns are independent, so parallel over columns. Version C: "parallel for" over columns j, each thread walks down its column (simple, but each step jumps W elements: poor cache use). Version B: each thread takes one contiguous block of columns (W\*t/p .. W\*(t+1)/p - 1, from its thread number t of p) and goes row by row inside it (i outer, j inner), so memory is read contiguously - the same order as the serial code. A thread only reads values of its own columns, which it wrote itself, so no barrier is needed between rows. Queries only read S and write their own answer: "parallel for" over queries. Integers -> results must match exactly.

*Source:* lab extension (prefix sums / image processing).

File: [`problems/p084_summed_area_table.c`](problems/p084_summed_area_table.c)

### P085 — Cellular automata (1D elementary rules, 2D majority vote)

**Problem.** (a) 1D elementary cellular automaton: N cells of 0/1, rule number R (0..255). In each generation every cell looks at (left, self, right) = a 3-bit number k = 4\*left + 2\*self + right and becomes bit k of R. Cells outside the array are always 0 (fixed boundary). Run G generations (e.g. rule 30, rule 90). (b) 2D majority vote on an H x W grid of 0/1 (row-major c[(long)i\*W + j], outside = 0): a cell becomes 1 if at least 5 of the 9 cells of its 3x3 neighbourhood (itself included) are 1, else 0; run S steps. Parallelise both with OpenMP, verify the final state against the serial code, and print the first generations of a small rule-90 run.

**Solution.** Generation g+1 is computed only from generation g, so with two arrays (double buffering: read cur, write next, then swap pointers) all cells of a generation are independent: "parallel for" over cells (1D) or "parallel for collapse(2)" over (i, j) (2D) is race-free. Generations are sequential (implicit barrier at the end of each parallel for; the swap is done by the master between generations). Integer states -> results must be identical to serial.

*Source:* lab extension (stencils / simulations).

File: [`problems/p085_cellular_automaton.c`](problems/p085_cellular_automaton.c)

### P086 — Mandelbrot set - escape-time image and area by random sampling

**Problem.** (Textbook Ex 22.1.) (a) For a W x H image of the region [-2, 0.5] x [-1.25, 1.25] of the complex plane, compute for every pixel c the number of iterations of z = z^2 + c (z0 = 0) until |z| > 2, at most max_iter (escape time), stored row-major img[(long)y\*W + x]. The work per pixel varies wildly (points inside the set take all max_iter iterations), so compare schedule(static), schedule(dynamic, 1) and schedule(guided) over rows. (b) Estimate the area of the Mandelbrot set by sampling random points in the same box: area ~ box area \* (points that never escape) / (number of samples); the true area is about 1.506. Verify the image against the serial code exactly and the area within a tolerance.

**Solution.** (a) every pixel is independent (it only writes img[y][x]), so a parallel for over rows is race-free; the question is load balance. Rows through the middle of the set cost up to max_iter per pixel, rows near the top/bottom almost nothing: static gives each thread one contiguous block of rows, so the threads holding the middle blocks finish last. dynamic, 1 hands out one row at a time to whichever thread is free; guided starts with big chunks and shrinks them. Each pixel uses the same code as the serial version, so the image is identical. (b) parallel for with reduction(+:inside). Random numbers: no rand() (not thread-safe, shared state). Sample k uses a counter-based generator: its two coordinates are a hash of (2k) and (2k+1). The sample set does not depend on the number of threads or the schedule, so the parallel count equals the serial count exactly.

*Source:* textbook Ex 22.1 (Mandelbrot).

File: [`problems/p086_mandelbrot.c`](problems/p086_mandelbrot.c)

## Tasks, recursion, graphs and applications

| # | Problem | File |
|---|---|---|
| P087 | Smallest factor of N with OpenMP tasks (early stop) | [`p087_smallest_factor_tasks.c`](problems/p087_smallest_factor_tasks.c) |
| P088 | Pascal's triangle (binomial coefficients) row by row | [`p088_pascal_triangle.c`](problems/p088_pascal_triangle.c) |
| P089 | Task dependencies with depend(in/out) (OpenMP 4.0) | [`p089_task_dependencies.c`](problems/p089_task_dependencies.c) |
| P090 | Recursive divide-and-conquer sum and max with tasks (cutoff) | [`p090_recursive_sum_tasks.c`](problems/p090_recursive_sum_tasks.c) |
| P091 | N-Queens solution count (parallel for and tasks) | [`p091_nqueens_tasks.c`](problems/p091_nqueens_tasks.c) |
| P092 | Adaptive Simpson quadrature with tasks | [`p092_adaptive_simpson_tasks.c`](problems/p092_adaptive_simpson_tasks.c) |
| P093 | All-pairs shortest paths (Floyd-Warshall) | [`p093_floyd_warshall.c`](problems/p093_floyd_warshall.c) |
| P094 | Level-synchronous parallel BFS | [`p094_bfs_level_sync.c`](problems/p094_bfs_level_sync.c) |
| P095 | Connected components by parallel label propagation | [`p095_connected_components.c`](problems/p095_connected_components.c) |
| P096 | N-body forces (full loop vs Newton's third law) | [`p096_nbody_forces.c`](problems/p096_nbody_forces.c) |
| P097 | K-means clustering | [`p097_kmeans.c`](problems/p097_kmeans.c) |
| P098 | Sieve of Eratosthenes (simple parallel and segmented) | [`p098_sieve_primes.c`](problems/p098_sieve_primes.c) |
| P099 | Perfect numbers and amicable pairs (load balancing) | [`p099_perfect_amicable.c`](problems/p099_perfect_amicable.c) |
| P100 | Longest Collatz chain (max-location reduction) | [`p100_collatz_longest.c`](problems/p100_collatz_longest.c) |
| P101 | User-defined reductions on structs | [`p101_struct_reduction.c`](problems/p101_struct_reduction.c) |
| P102 | Word-length histogram, longest word and sentence count | [`p102_word_length_histogram.c`](problems/p102_word_length_histogram.c) |
| P103 | Wavefront dynamic programming (matrix chain, LCS) | [`p103_matrix_chain_dp.c`](problems/p103_matrix_chain_dp.c) |
| P104 | Three-stage pipeline with task dependences | [`p104_pipeline_sections_tasks.c`](problems/p104_pipeline_sections_tasks.c) |

### P087 — Smallest factor of N with OpenMP tasks (early stop)

**Problem.** (Textbook Ex 24.1.) Find the smallest factor f >= 2 of a number N by trial division (f = 2, 3, ... up to sqrt(N); if none divides, N is prime and the answer is N), e.g. N = 2999 \* 3001. Create one task per trial factor (or per block of factors) from a single thread, record the smallest factor found in a shared variable, and stop creating tasks once a factor is known. Compare with a "parallel for" + reduction(min:). Test several numbers, including a prime, and verify against the serial loop.

**Solution.** One thread (single) walks the candidates in blocks of B and makes a task per block; the other threads of the team execute the tasks. A task scans its block upward and, at its first divisor f, does "factor = min(factor, f)" inside a critical section (read-compare-write must be one atomic step: OpenMP 4.5 has no atomic min). Before creating the next block the generator reads factor (also in the critical section); if factor <= start of the block, every remaining candidate is larger, so it stops. Tasks that start after a smaller factor is known skip their block. Correct because the block holding the true smallest factor always starts below any factor found, so it is always created and scanned. The parallel for version cannot leave the loop early (no break in an omp for), so it always tests every candidate up to sqrt(N); reduction(min:best) gives each thread a private best = N and takes the minimum at the end.

*Source:* textbook Ex 24.1 (tasks).

File: [`problems/p087_smallest_factor_tasks.c`](problems/p087_smallest_factor_tasks.c)

### P088 — Pascal's triangle (binomial coefficients) row by row

**Problem.** (Textbook Ex 24.4.) Compute the binomial coefficients C(r, c), 0 <= c <= r <= R, in a triangular array, where row r comes from row r-1: C(r, 0) = C(r, r) = 1 and C(r, c) = C(r-1, c-1) + C(r-1, c). Work modulo M = 1000000007 so the numbers stay bounded. Store the triangle in one array: row r starts at offset r(r+1)/2, so C(r, c) = T[r(r+1)/2 + c]. Parallelise with OpenMP (rows are sequential, the entries of a row are parallel); also compute only row R with two row buffers, and build a small triangle with tasks and depend clauses. Verify against the serial code and against the closed form C(r, c) = r! / (c! (r-c)!).

**Solution.** Row r needs the whole of row r-1 (sequential in r), but its entries only read row r-1, so they are independent: "omp for" over c inside one parallel region; its implicit barrier finishes row r before row r+1 starts. Single row R: keep two buffers, row r in buf[r % 2], reading buf[(r-1) % 2] (never update one row in place in parallel: entry c would read entry c-1 that another thread may already have overwritten). Tasks: one task per entry with depend(in: the two entries above) depend(out: this entry); the runtime starts a task as soon as its inputs are done (no row barrier), but one addition per task is far too little work to pay for a task. Integers mod M -> results must match exactly.

*Source:* textbook Ex 24.4.

File: [`problems/p088_pascal_triangle.c`](problems/p088_pascal_triangle.c)

### P089 — Task dependencies with depend(in/out) (OpenMP 4.0)

**Problem.** (Textbook 24.7.1.) (a) Four steps on arrays form a small DAG: A: a[i] = 0.001\*i;   B: b[i] = sin(a[i]) (needs A);   C: c[i] = cos(a[i]) (needs A);   D: d[i] = b[i]^2 + c[i]^2 (needs B and C; so d[i] = 1). Run each step as an OpenMP task and express the order with "depend(in: ...) depend(out: ...)" so that B and C can run at the same time; compare with taskwait. (b) Fill a Fibonacci array f[0..n-1] with one task per entry, task i computing f[i] = f[i-1] + f[i-2]. Show that WITHOUT depend clauses the tasks run in any order and the result is wrong, and that WITH depend it is always correct. Verify against serial.

**Solution.** A task with depend(out: x) must finish before any later-created task with depend(in: x) or depend(out: x) starts; tasks with only depend(in: x) on the same x may run together. So A: out(a)   B: in(a) out(b)   C: in(a) out(c)   D: in(b, c) out(d) gives exactly the DAG A -> {B, C} -> D. The dependence items here are the array sections a[0:n], b[0:n], ... (always the same section per array). Fibonacci: task i has depend(in: f[i-1], f[i-2]) depend(out: f[i]), a chain. Without depend, a task may read f[i-1] before it is written; to show this SAFELY (no data race = no undefined behaviour) that version reads and writes f with "omp atomic read/write" and starts from f = -1, so a too-early task simply uses a wrong value and the error is visible. Tasks are created by one thread (single); the barrier at the end of the parallel region waits for all of them.

*Source:* textbook 24.7.1 (task dependencies); lab extension.

File: [`problems/p089_task_dependencies.c`](problems/p089_task_dependencies.c)

### P090 — Recursive divide-and-conquer sum and max with tasks (cutoff)

**Problem.** Compute the sum and the maximum of an array a[N] of doubles by recursive divide and conquer: split the range in two halves, solve each half (as an OpenMP task), wait for both, combine (sum = left + right, max = larger of the two). Below a cutoff length solve the range with a plain loop. Compare with "parallel for reduction(+:sum) reduction(max:mx)" and the serial loop, verify, and show how the cutoff affects the time.

**Solution.** One thread (single) calls the recursive function; at each level it creates a task for the left half and one for the right half, then "taskwait" waits for these two children before combining. Each task writes its result into a variable of the parent declared shared(...) (task variables are firstprivate by default, so without shared the result would go into the task's own copy and be lost); the parent's variables stay alive until the taskwait. Idle threads of the team pick up the queued tasks. The cutoff controls task granularity: too small = huge numbers of tiny tasks (overhead), too large = too few tasks to keep all threads busy. Max is exact in any order; the sum is added in a different order, so compare it with a tolerance.

*Source:* lab extension (tasks, recursive parallelism).

File: [`problems/p090_recursive_sum_tasks.c`](problems/p090_recursive_sum_tasks.c)

### P091 — N-Queens solution count (parallel for and tasks)

**Problem.** Count all the ways to place N queens on an N x N chess board so that no two queens attack each other (same column or same diagonal), for N = 8..13. Use backtracking with bitmasks. Parallelise it with OpenMP in two ways: (a) a parallel for over the columns of the first-row queen, (b) tasks, one per placement of the queens in the first two rows. Verify against the known counts (8 -> 92, 10 -> 724, 12 -> 14200, 13 -> 73712) and the serial result, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The search tree below different first-row (or first-two-row) placements are independent subtrees, so they can be searched in parallel; each subtree only reads its own copies of the masks (passed by value), so there is nothing shared except the count. (a) parallel for over the first column with reduction(+:count); subtrees have very different sizes, so schedule(dynamic,1) hands out one column at a time. (b) parallel + single starts the recursion; while row < TASK_ROWS every safe column becomes a task that writes its count into its own slot sub[c]; taskwait, then add the slots. Below the cutoff the search is plain serial recursion.

*Source:* classic backtracking exercise (task parallelism, irregular trees).

File: [`problems/p091_nqueens_tasks.c`](problems/p091_nqueens_tasks.c)

### P092 — Adaptive Simpson quadrature with tasks

**Problem.** Compute I = integral from 0 to 1 of f(x) = sqrt(x) + 1/(1 + 1000(x - 0.3)^2) with adaptive Simpson quadrature to a tolerance eps: apply Simpson's rule on [a,b] and on its two halves; if the two estimates differ by more than 15\*eps, split the interval and recurse on each half with eps/2 (so the method refines only near the sharp peak at x = 0.3 and the sqrt corner at x = 0). Parallelise the recursion with OpenMP tasks (left and right halves become tasks above a depth cutoff). Compare with the exact value 2/3 + (atan(0.7 sqrt(1000)) + atan(0.3 sqrt(1000))) / sqrt(1000) and with the serial result, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The two halves of an interval are independent sub-problems (each gets its endpoints, f values and eps by value), so each half can be a task. parallel + single starts the recursion; below depth TASK_DEPTH the recursion is serial (cutoff). Two ways to combine the results: (a) each task returns its value into a local of the parent (shared(l), shared(r)); the parent does taskwait and returns l + r. Same tree and same order of additions as the serial code, so the result is identical. (b) no waiting inside the recursion: an accepted piece is added into one shared total with #pragma omp atomic; the barrier at the end of the parallel region waits for all tasks. Balances better with gcc (see Notes); the order of additions varies, so compare with a tolerance.

*Source:* recursive divide-and-conquer with tasks (lecture slides on tasks).

File: [`problems/p092_adaptive_simpson_tasks.c`](problems/p092_adaptive_simpson_tasks.c)

### P093 — All-pairs shortest paths (Floyd-Warshall)

**Problem.** Given a dense weighted directed graph on n vertices as an n x n matrix d (d[i][j] = weight of edge i->j, INF if there is no edge, 0 on the diagonal), compute the length of the shortest path between every pair of vertices with the Floyd-Warshall algorithm: for k: for i: for j: d[i][j] = min(d[i][j], d[i][k] + d[k][j]) Parallelise it with OpenMP, verify against the serial version on a random graph, print the result for a small 4-vertex example, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The k loop must stay sequential: step k uses the matrix produced by step k-1. For a fixed k the (i, j) updates are independent: they read row k and column k, which do not change during step k (d[i][k] + d[k][k] = d[i][k] because d[k][k] = 0, so no update of row/column k is ever an improvement), and each d[i][j] is written by the one thread that owns row i. So the i loop is a parallel for, with a barrier between steps. (a) #pragma omp parallel for on the i loop inside the k loop (n fork/joins). (b) one parallel region around the k loop; every thread runs the k loop and #pragma omp for splits the i loop; its implicit barrier ends step k.

*Source:* classic dynamic-programming / graph exercise (lab extension).

File: [`problems/p093_floyd_warshall.c`](problems/p093_floyd_warshall.c)

### P094 — Level-synchronous parallel BFS

**Problem.** Given an undirected graph with n vertices in CSR format (the neighbours of u are adj[off[u]] .. adj[off[u+1]-1]) and a source vertex s, compute dist[v] = number of edges on a shortest path from s to v (-1 if v cannot be reached) by breadth-first search. Parallelise it level by level: all vertices of the current frontier are expanded in parallel, every unvisited neighbour must be claimed exactly once and appended to the next frontier. Verify the distances against a serial BFS and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Levels are processed one after another (level L+1 needs the whole level L); within a level the frontier is split with parallel for schedule(dynamic, 64) (vertex degrees differ). Two threads may find the same unvisited neighbour v, so the claim is an atomic exchange on visited[v] (#pragma omp atomic capture: old = visited[v]; visited[v] = 1): only the thread that sees old == 0 owns v, writes dist[v] and appends v. (a) append with an atomic index: #pragma omp atomic capture pos = nsize++. (b) each thread appends to its own buffer; at the end of the level it reserves room in the next frontier with ONE atomic capture and copies its buffer there (far fewer atomics on the shared counter).

*Source:* graph-algorithms lab extension (level-synchronous BFS).

File: [`problems/p094_bfs_level_sync.c`](problems/p094_bfs_level_sync.c)

### P095 — Connected components by parallel label propagation

**Problem.** An undirected graph with n vertices is given as an edge list (eu[e], ev[e]), e = 0..m-1. Find its connected components by label propagation: every vertex starts with its own number as label; repeatedly set label[u] = label[v] = min(label[u], label[v]) over all edges until no label changes. Then label[v] = smallest vertex of v's component. Parallelise the sweeps with OpenMP, verify against a serial union-find, print the number of components and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Sweeps are sequential (repeat until no change); one sweep is a parallel for whose "did anything change?" flag is a reduction(||:changed). Two ways to make one sweep race-free: (a) double buffer (Jacobi): build the adjacency lists (CSR) once; in a sweep every vertex v computes new[v] = min(old[v], old[u] for neighbours u) reading only old[] and writing only its own new[v]; swap buffers. (b) in place on the edge list (monotone atomic min): read both labels with atomic read; lower the bigger one inside a critical section that re-checks (label = min(label, val)) - labels only ever decrease, so a stale read just means one more sweep. Fewer sweeps, but each decrease pays for the critical section. Timing baseline: the same propagation run serially (in place). The check uses a serial union-find that links the bigger root under the smaller, so every root is the smallest vertex of its component (same canonical label).

*Source:* graph-algorithms lab extension (label propagation / union-find).

File: [`problems/p095_connected_components.c`](problems/p095_connected_components.c)

### P096 — N-body forces (full loop vs Newton's third law)

**Problem.** N particles in 2D have positions (x[i], y[i]) and masses m[i]. The gravitational force on particle i (with softening eps, G = 1) is F_i = sum over j != i of m_i m_j (r_j - r_i) / (|r_j - r_i|^2 + eps^2)^(3/2). Compute all forces with OpenMP in two ways: (a) the full O(N^2) double loop without Newton's third law; (b) using symmetry F_ij = -F_ji: loop over j > i only, add the pair force to particle i and subtract it from particle j. Make (b) race-free in two ways: (b1) atomics, (b2) per-thread force arrays reduced at the end. Verify all versions against the serial full loop, and compare the times. Why does (b) do half the work but need synchronisation?

**Solution.** (a) parallel for over i: iteration i writes only fx[i], fy[i], so there is no race (each thread owns its rows); schedule(static) since every row costs N-1 pair forces. (b) iteration i also writes fx[j], fy[j] for every j > i, and another thread (another i) may be updating the same j: a race. (b1) each fx[j] -= ... is #pragma omp atomic, and the row total for i is added with one atomic at the end (row i may also be another thread's j). (b2) thread t accumulates into its own arrays fxt[t][.], fyt[t][.] (no sharing), then a second parallel for adds the T arrays for each particle. The triangular loop (row i has N-1-i pairs) is unbalanced: rows get cheaper as i grows, so (b) uses schedule(dynamic, 16) (see Notes on guided).

*Source:* textbook ch. 30 (n-body force computation).

File: [`problems/p096_nbody_forces.c`](problems/p096_nbody_forces.c)

### P097 — K-means clustering

**Problem.** Cluster N points in 2D into K clusters with K-means for a fixed number of iterations. Start from K deterministic centroids (points 0, N/K, 2N/K, ...). Each iteration: (1) assignment: give every point the label of its nearest centroid (ties -> smaller cluster number); (2) update: move each centroid to the mean of its points (a cluster with no points keeps its centroid). Parallelise with OpenMP, verify labels and centroids against the serial code, and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The iterations are sequential (each needs the previous centroids). Assignment: parallel for over points; point i writes only label[i], the centroids are only read - no race. Update: many points add into the same sx[c], sy[c], cnt[c], so the sums need a reduction: (a) array-section reduction: reduction(+:sx[:K], sy[:K], cnt[:K]) gives every thread private zeroed arrays and adds them at the end (OpenMP 4.5). (b) by hand: one parallel region per iteration; each thread assigns its points and sums into local arrays, then adds them into the shared arrays inside a critical section (K adds per thread, not per point). The K divisions are done serially. Point coordinates are whole numbers, so all sums are exact in any order and the parallel centroids match the serial ones exactly (with real-valued data compare centroids with a tolerance).

*Source:* data-parallel clustering lab extension (array reductions).

File: [`problems/p097_kmeans.c`](problems/p097_kmeans.c)

### P098 — Sieve of Eratosthenes (simple parallel and segmented)

**Problem.** Count the primes <= N with the sieve of Eratosthenes (for every prime p <= sqrt(N), cross out p\*p, p\*p+p, p\*p+2p, ... <= N; what is left is prime). Parallelise it in two ways: (a) the simple sieve with the marking loop of each prime done in parallel; (b) a segmented sieve: split [2, N] into blocks, every thread sieves its own blocks using the base primes <= sqrt(N). Verify the counts against the serial sieve and the known values (pi(1e7) = 664579, pi(1e8) = 5761455) and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** (a) the loop over p stays serial (comp[p] must be final before we know p is prime - it is, because all smaller primes were done); for one p the multiples j = p\*p, p\*p+p, ... are distinct, so "#pragma omp parallel for" over j writes each comp[j] from exactly one thread: no race; the implicit barrier ends prime p. The final count is a reduction(+:count). But every prime sweeps the whole N-byte array, so it is memory-bound. (b) base primes <= sqrt(N) come from a small serial sieve. Blocks are independent: a thread sieves block [lo, hi] in its own private buffer (sized to fit in cache) starting each prime at its first multiple >= lo, counts the zeros, and adds to reduction(+:count). Nothing is shared but the read-only base primes. schedule(dynamic) hands out whole blocks.

*Source:* classic sieve exercise (lab extension; segmented sieve).

File: [`problems/p098_sieve_primes.c`](problems/p098_sieve_primes.c)

### P099 — Perfect numbers and amicable pairs (load balancing)

**Problem.** Let s(k) be the sum of the proper divisors of k (divisors < k), found by trial division up to sqrt(k). Find all perfect numbers below N (s(k) = k) and all amicable pairs a < b below N (s(a) = b and s(b) = a). The cost of s(k) grows like sqrt(k), so the work per number is uneven: compare schedule(static) with schedule(dynamic). Verify against the serial code (perfect numbers below 1e5: 6, 28, 496, 8128) and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Step 1 computes the table s[k] for all k < N: iteration k writes only s[k], so a parallel for has no race. Only the schedule matters: (a) schedule(static): thread t gets one contiguous block; the last block holds the largest (most expensive) numbers, so the others wait for it. (b) schedule(static, 1): cyclic, thread t gets k = t, t+T, ... - every thread gets a mix of small and large k (good here because cost grows smoothly). (c) schedule(dynamic, 256): idle threads grab the next chunk of 256 numbers. Step 2 (cheap, O(N)) collects the results with one ordered serial pass over s[], so the lists come out sorted with no shared list to protect.

*Source:* number-theory load-balancing exercise (lab extension).

File: [`problems/p099_perfect_amicable.c`](problems/p099_perfect_amicable.c)

### P100 — Longest Collatz chain (max-location reduction)

**Problem.** The Collatz sequence of n repeats n -> n/2 (n even) or n -> 3n+1 (n odd) until it reaches 1. Its length is the number of terms including n and 1 (length(13) = 10: 13 40 20 10 5 16 8 4 2 1). Find the starting number below N with the longest chain (ties -> the smaller start) using OpenMP. The work per number is very uneven. Verify against the serial result (below 1e6: 837799, length 525) and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** The chain lengths are independent, so the loop over starts is a parallel for; schedule(dynamic, 1000) because chain lengths vary wildly. The answer is a (length, start) pair - a max-location, which the built-in reduction(max:) cannot do. Two ways: (a) every thread keeps its own best pair; at the end each thread merges it into the shared best inside a critical section (once per thread). (b) #pragma omp declare reduction(maxloc : Best : ...) with a combiner that keeps the better pair and an initializer {0, 0}; then reduction(maxloc:best) on the parallel for. Both use the same better() test: longer wins, equal length -> smaller start, so the answer does not depend on how the work was split.

*Source:* Project Euler 14 style load-balancing exercise (lab extension).

File: [`problems/p100_collatz_longest.c`](problems/p100_collatz_longest.c)

### P101 — User-defined reductions on structs

**Problem.** Use OpenMP user-defined reductions (declare reduction) to compute, each in ONE parallel loop with ONE reduction variable of struct type: (a) the complex sum S = sum over k = 1..N of e^(i k t) / k for t = 1, with a struct {double re, im;} (the series tends to -ln(2 sin(t/2)) + i(pi - t)/2); (b) the centroid of the N random 3D points that lie inside the unit sphere (sum of x, y, z and the count in one struct); (c) the bounding box of all N points (min and max of x, y and z in one struct). Verify against serial loops and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Reduction(+:) only works on arithmetic types. For a struct we declare #pragma omp declare reduction(name : Type : combiner) initializer(...): every thread gets a private copy omp_priv set by the initializer (the identity: zeros for sums, +inf/-inf for min/max), updates it with no sharing, and at the end the copies are merged pairwise with the combiner (omp_out = merge(omp_out, omp_in)). Then reduction(name:var) on the parallel for is race-free exactly like reduction(+:s).

*Source:* lecture slides on reductions (user-defined reductions, OpenMP 4.0+).

File: [`problems/p101_struct_reduction.c`](problems/p101_struct_reduction.c)

### P102 — Word-length histogram, longest word and sentence count

**Problem.** A large text of N characters is given (generated here: lowercase words separated by spaces and newlines, some followed by '.'). A word is a maximal run of letters. Compute with OpenMP: the histogram of word lengths (lengths 1..20, and one bin for 21 or more), the longest word (its length and starting position; ties -> the first one) and the number of sentences (number of '.' characters). Words may cross the boundary between two threads' chunks and must be counted exactly once. Verify against the serial code and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Position i is a word START if t[i] is a letter and t[i-1] is not (or i = 0). Only the thread that owns position i counts that word; it reads forward (past its own chunk if needed) to measure the length. A thread whose chunk begins in the middle of a word sees that t[lo-1] is a letter, so it does not count it. All reads of t are read-only, so the only shared results are the counters: (a) parallel for over positions: reduction(+:hist, sentences) for the counts (array reduction), per-thread longest word merged in a critical section. (b) explicit chunks: thread t scans [t\*N/T, (t+1)\*N/T) into a private Stats struct, then merges it into the shared one in a critical section.

*Source:* text-processing lab extension (chunk boundaries).

File: [`problems/p102_word_length_histogram.c`](problems/p102_word_length_histogram.c)

### P103 — Wavefront dynamic programming (matrix chain, LCS)

**Problem.** Parallelise two dynamic programs with OpenMP. (1) Matrix-chain order: matrices A1..An, Ai is p[i-1] x p[i]. The fewest scalar multiplications for Ai..Aj is m[i][i] = 0, m[i][j] = min over i <= k < j of m[i][k] + m[k+1][j] + p[i-1] p[k] p[j]. Also record the best split s[i][j] and print the optimal parenthesisation for p = {30, 35, 15, 5, 10, 20, 25} (answer 15125). (2) Longest common subsequence of strings a (length n) and b (length m): c[i][j] = c[i-1][j-1] + 1 if a[i-1] = b[j-1], else max(c[i-1][j], c[i][j-1]). Verify the parallel tables against the serial ones and report time and speedup for 1, 2, 4 and 8 threads.

**Solution.** Find cells that do not depend on each other (a "wavefront"). (1) m[i][j] needs only shorter chains (m[i][k], m[k+1][j] with length < L = j-i+1), so all cells with the same chain length L are independent: loop L = 2..n sequentially, parallel for over i (j = i+L-1). Each cell is written by one thread; the implicit barrier finishes length L before L+1. (2) c[i][j] needs cells on the two previous anti-diagonals (i+j = d-1, d-2), so cells with the same d = i+j are independent: (2a) loop d sequentially, omp for over i on each anti-diagonal (one parallel region, barrier per diagonal); (2b) tiled wavefront: the same idea on B x B tiles - tile (I,J) needs tiles (I-1,J), (I,J-1), (I-1,J-1), so tiles with the same I+J run in parallel and each tile is filled row by row (cache-friendly, n/B times fewer barriers).

*Source:* textbook dynamic programming (CLRS 15.2 and 15.4), wavefront pattern.

File: [`problems/p103_matrix_chain_dp.c`](problems/p103_matrix_chain_dp.c)

### P104 — Three-stage pipeline with task dependences

**Problem.** Data arrives in B blocks of S values and goes through 3 stages: stage 1 generates block b, stage 2 transforms it, stage 3 reduces it to a block sum r[b] and outputs the running total out[b] = out[b-1] + r[b]. Block b's stage s needs block b's stage s-1, and each stage handles the blocks in order (stage s of block b-1 must finish before stage s of block b). (a) Run it as a pipeline with OpenMP tasks and depend clauses, so different stages of different blocks overlap. (b) Compare with running the stages one after another, each as a parallel for over the values of one block. (c) Also build the pipeline with parallel sections (one section per stage). Verify out[] against the serial code and report time and speedup.

**Solution.** (a) one thread (single) creates 3 tasks per block; the runtime starts a task when its dependences are met: stage 1: depend(inout: tok[0])                    depend(out: done1[b]) stage 2: depend(inout: tok[1]) depend(in: done1[b]) depend(out: done2[b]) stage 3: depend(inout: tok[2]) depend(in: done2[b]) done1[b], done2[b] are per-block sentinels (stage s-1 -> stage s of block b); tok[s] is a per-stage sentinel: inout on the same item chains the stage-s tasks in creation order, i.e. block order. So while stage 3 works on block b, stage 2 can work on b+1 and stage 1 on b+2: at most 3 tasks run at once, the speedup is at most 3 (number of stages). Each block has its own buffers X[b], Y[b], so overlapping stages never touch the same data. (b) data parallelism inside each stage: per block, parallel for over the S values for stages 1 and 2, and a reduction(+:sum) for stage 3; this scales with the threads but pays a fork/join per stage per block. (c) parallel sections num_threads(3): section s runs stage s for b = 0..B-1; it waits (spins) until stage s-1 has finished block b, read from a progress counter written with atomic write after a flush (producer) and read with atomic read followed by a flush (consumer), so the block data is visible.

*Source:* lecture slides on tasks (depend clauses, OpenMP 4.0+), pipeline pattern.

File: [`problems/p104_pipeline_sections_tasks.c`](problems/p104_pipeline_sections_tasks.c)
