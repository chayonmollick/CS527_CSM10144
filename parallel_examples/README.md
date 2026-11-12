# Parallel program examples (OpenMP and the mini-computer)

Short, tested examples of every parallel idea in CS527, for the final lab test.

- [`openmp/`](openmp/): one small C program per OpenMP construct (22 programs, at most 70
  lines each). Each one starts with a comment saying what the construct does, when to use
  it and what output to expect (and which parts vary between runs). Each checks itself
  against a serial version and prints `OK`.
- [`minicomputer/`](minicomputer/): parallel programs for the lab6 simulator. One job is
  split across several tasks, the simulated OS runs them at the same time on its 4
  processors, and their results are combined at the end.

## Run them
```bash
cd parallel_examples/openmp
./run.sh                 # make + run all 22 with OMP_NUM_THREADS=4  ->  22 passed, 0 failed
./run.sh 12              # only the examples whose name contains 12, with their output
make 05_race_vs_critical_vs_atomic && OMP_NUM_THREADS=4 ./05_race_vs_critical_vs_atomic
gcc -O2 -Wall -Wextra -fopenmp 03_reduction_sum.c -o 03_reduction_sum -lm   # one file, no make
make clean

cd ../minicomputer
./run.sh                 # builds ../../lab6/sim if needed, runs (a)-(e)  ->  23 passed, 0 failed
```
The OpenMP programs use only C99 and OpenMP 4.5, so the gcc 9 on a lab PC can build them.
They were checked with gcc 15 (also with `-std=c99 -pedantic`) and with clang + libomp:
no warnings under `-O2 -Wall -Wextra`, and every check passes.

## OpenMP: one example per construct ([`openmp/`](openmp/))
| File | Construct | What it shows |
|---|---|---|
| [`01_hello_threads.c`](openmp/01_hello_threads.c) | `parallel`, `omp_get_thread_num`, `omp_get_num_threads`, `num_threads(n)` | Every thread runs the block; who decides the team size |
| [`02_parallel_for.c`](openmp/02_parallel_for.c) | `parallel for` | Which thread gets which iterations (`0 0 0 0 1 1 1 1 ...`); independent iterations only |
| [`03_reduction_sum.c`](openmp/03_reduction_sum.c) | `reduction(+:...)` | Sum, double sum and count in one loop; the reduction adds to the old value |
| [`04_reduction_max_min.c`](openmp/04_reduction_max_min.c) | `reduction(max:)`, `reduction(min:)`, `critical` | Max/min value; max **with its index** (per-thread best, one critical merge) |
| [`05_race_vs_critical_vs_atomic.c`](openmp/05_race_vs_critical_vs_atomic.c) | `critical`, `atomic` | `count++` loses updates; the cost of critical, atomic and reduction |
| [`06_locks.c`](openmp/06_locks.c) | `omp_lock_t` | One lock per histogram bin; take several locks in a fixed order |
| [`07_private_firstprivate_lastprivate.c`](openmp/07_private_firstprivate_lastprivate.c) | `private`, `firstprivate`, `lastprivate` | The value before, inside and after the region |
| [`08_shared_default_none.c`](openmp/08_shared_default_none.c) | `shared`, `default(none)` | Every variable listed; the error you get when one is missing |
| [`09_single_master_barrier.c`](openmp/09_single_master_barrier.c) | `single`, `master`, `barrier` | Set-up once; two phases separated by a barrier |
| [`10_nowait.c`](openmp/10_nowait.c) | `for nowait` | When removing the implicit barrier is safe, and when it is not |
| [`11_sections.c`](openmp/11_sections.c) | `parallel sections` | Three different jobs at the same time |
| [`12_schedule_static_dynamic_guided.c`](openmp/12_schedule_static_dynamic_guided.c) | `schedule(static/dynamic/guided/runtime)` | Uneven work (prime test); why `static,1` is the slowest here |
| [`13_collapse.c`](openmp/13_collapse.c) | `collapse(2)` | 3 rows for 4 threads: 3 busy threads without it, 4 with it |
| [`14_ordered.c`](openmp/14_ordered.c) | `ordered` | Compute in parallel, print in loop order |
| [`15_tasks_fibonacci.c`](openmp/15_tasks_fibonacci.c) | `task`, `taskwait`, cutoff | Recursion with tasks; `shared(x)` in a task; task count |
| [`16_taskwait_taskgroup.c`](openmp/16_taskwait_taskgroup.c) | `taskwait` vs `taskgroup` | Children only vs all descendants |
| [`17_simd.c`](openmp/17_simd.c) | `simd`, `parallel for simd` | Vector lanes inside each thread |
| [`18_threadprivate.c`](openmp/18_threadprivate.c) | `threadprivate`, `copyin` | A global with one copy per thread, kept between regions |
| [`19_nested_parallelism.c`](openmp/19_nested_parallelism.c) | nested `parallel`, `omp_set_max_active_levels` | 2 x 2 threads with nesting, 2 x 1 without |
| [`20_timing_speedup.c`](openmp/20_timing_speedup.c) | `omp_get_wtime` | Time, speedup, efficiency and Karp-Flatt table for 1, 2, 4, 8 threads |
| [`21_false_sharing.c`](openmp/21_false_sharing.c) | (performance) | Per-thread sums side by side vs padded vs local |
| [`22_array_reduction.c`](openmp/22_array_reduction.c) | `reduction(+:a[:n])` | Histogram and column sums with array-section reductions |

`openmp/run.sh` passes an example when it exits with 0 and prints `OK` (and no `WRONG`).
Race results, timings, thread numbers and line order vary between runs; each file says which.

## Mini-computer: one job, several tasks ([`minicomputer/`](minicomputer/))
The lab6 OS runs up to 4 tasks at once, one per simulated processor, and gives each a
time slice of 10 instructions per round (round robin). A parallel job here is the **same
program run as several tasks**, each with its own data file holding its part of the data
(data decomposition, like an MPI scatter). Each task leaves its result in its data memory,
which the OS saves as `<data file>_<pid>.out` when the task ends. The results are then
combined by [`combine.sh`](minicomputer/combine.sh) (bash + awk), or by one more task
whose data file is built from them.

| Example | Program | Data files | Each task computes | Combined |
|---|---|---|---|---|
| (a) array sum | [`sum_part.txt`](minicomputer/sum_part.txt) | `sum1..sum4.byte` (8 elements each); `sum_all.byte` = the serial run | partial sum at [4] | `combine.sh sum` = -462, or `sum_part.txt` run again on `partials.byte` |
| (b) dot product | [`dot_part.txt`](minicomputer/dot_part.txt) | `dot1..dot4.byte` (8 elements of A and of B each) | partial A·B at [4] | `combine.sh sum` = 147 |
| (c) y = A x, 8x8 | [`matvec_rows.txt`](minicomputer/matvec_rows.txt) | `matvec1..matvec4.byte` (2 rows of A + x each); `matvec_all.byte` = serial | 2 values of y, with vector instructions | `combine.sh matvec` = `8 -22 25 -38 20 -10 26 -26` |
| (d) search | [`search_part.txt`](minicomputer/search_part.txt) | `search1..search4.byte` (key 42, at indices 13 and 27) | first global index of the key in its chunk, or -1 | `combine.sh search` = 13 (smallest result that is not -1) |
| (e) 5 tasks, 4 processors | [`sum_part.txt`](minicomputer/sum_part.txt) | `sum1..sum5.byte` | as (a); `log.txt` shows the waiting queue and the round robin | `combine.sh sum` = -497 |

Each program's header comment gives its data layout and the expected values. Data files hold
32-bit little-endian words, 4 hex bytes per line: `0A 00 00 00` = 10, `FF FF FF FF` = -1.
[`run.sh`](minicomputer/run.sh) runs everything in a temporary directory and compares the
results with answers it computes from the input files with awk, without the simulator.

### Run them by hand
```bash
cd parallel_examples/minicomputer
(cd ../../lab6 && make)                 # the simulator
SIM=../../lab6/sim
rm -f log.txt                           # print output is appended to log.txt

# (a) 4 tasks, then combine on the host, or with a 5th run of the same program
$SIM -b sum_part.txt sum1.byte sum_part.txt sum2.byte sum_part.txt sum3.byte sum_part.txt sum4.byte
./combine.sh sum sum1_1.out sum2_2.out sum3_3.out sum4_4.out                # -462
./combine.sh partials sum1_1.out sum2_2.out sum3_3.out sum4_4.out > partials.byte
$SIM -b sum_part.txt partials.byte && ./combine.sh sum partials_1.out       # -462, computed by a task
$SIM -b sum_part.txt sum_all.byte                                           # serial: 1 task, 264 instructions

# (b), (c), (d)
$SIM -b dot_part.txt dot1.byte dot_part.txt dot2.byte dot_part.txt dot3.byte dot_part.txt dot4.byte
./combine.sh sum dot1_1.out dot2_2.out dot3_3.out dot4_4.out                # 147
$SIM -b matvec_rows.txt matvec1.byte matvec_rows.txt matvec2.byte matvec_rows.txt matvec3.byte matvec_rows.txt matvec4.byte
./combine.sh matvec matvec1_1.out matvec2_2.out matvec3_3.out matvec4_4.out # 8 -22 25 -38 20 -10 26 -26
$SIM -b search_part.txt search1.byte search_part.txt search2.byte search_part.txt search3.byte search_part.txt search4.byte
./combine.sh search search1_1.out search2_2.out search3_3.out search4_4.out # 13

# (e) 5 tasks on 4 processors
rm -f log.txt
$SIM -b sum_part.txt sum1.byte sum_part.txt sum2.byte sum_part.txt sum3.byte sum_part.txt sum4.byte sum_part.txt sum5.byte
cat log.txt

rm -f *.out *.freq program.byte log.txt partials.byte                       # clean up
```
The pid in an output file name is the task's position on the command line (1, 2, ...).

### What (e) shows
```
[os] pid 4 (sum_part) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 5 created: sum_part.txt sum5.byte (14 instructions)
[os] pid 5 is waiting for a free processor/frames                  <- waiting queue
[os] pid 1 (sum_part) finished on processor 0 after 72 instructions, ...
...
[os] pid 5 (sum_part) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
```
`log.txt` (`Process id: <pid>   x3 : <running sum in hex>`):
```
Process id: 1   x3 : FFFFFEEF      round 1: each task runs 10 instructions and prints once
Process id: 2   x3 : FFFFFEBE
Process id: 3   x3 : 00000150
Process id: 4   x3 : 0000011F
Process id: 1   x3 : FFFFFEE1      round 2, and so on
...
Process id: 1   x3 : FFFFFF43      round 5: two prints in one slice (the loop is 8 instructions)
Process id: 1   x3 : 0000007E
...
Process id: 5   x3 : 000000EE      rounds 9-16: pid 5 runs alone (the last 8 lines)
```

### Time and speedup, counted in rounds
In lab6 one host thread runs the processors' slices one after another, so the parallelism
is simulated: count time in rounds of 10 instructions. A task of `sum_part.txt` executes
8N + 8 instructions.
- (a) serial, 1 task with all 32 elements: 264 instructions = **27 rounds**. Parallel, 4 tasks
  of 8 at the same time: 72 instructions each = **8 rounds**, plus **4 rounds** for the
  combining task (40 instructions). Speedup 27 / 12 = 2.25, or 27 / 8 = 3.4 without the
  combine step: the serial combine step and the set-up that every task repeats are Amdahl's
  serial fraction.
- (e) 5 equal tasks on 4 processors: pid 5 starts only in round 9, when a processor is free,
  and then runs alone, so the job takes 16 rounds. The same 40 elements as 4 chunks of 10
  would take 9 rounds. That is load imbalance: use a number of equal tasks that is a multiple
  of the processor count, or many small ones.

### Why the tasks cannot share memory in lab6
- Every task has its **own page table** (in physical frame 0, found through `PTBR[p]`). The
  loader maps the task's logical pages to frames taken from the free list, never to another
  task's frames, so address 4 of task 1 and address 4 of task 2 are different physical bytes.
  No instruction can name another task's memory, and no OS call maps a frame into two tasks.
- When a task ends, `finalize()` writes its data pages to `<data>_<pid>.out` and frees its
  frames. Its result survives only in that file. That is why the results are combined outside
  the simulated machine (`combine.sh`) or by a new task whose data file is built from them
  (`partials.byte`).
- For tasks to cooperate while they run, the OS has to provide one of two things:
  - **Shared memory:** two page tables that map the same frame. [`q087_shared_page`](../final_practice/q087_shared_page/README.md)
    maps data page 7 of every task to one frame, and a reader waits on a flag word written by
    a writer. Updates from several tasks then need synchronisation, for example semaphores
    ([`q086_semaphores`](../final_practice/q086_semaphores/README.md)), exactly like threads in OpenMP.
  - **Message passing:** `send` / `recv` through the OS. [`q088_message_passing`](../final_practice/q088_message_passing/README.md)
    gives each task a mailbox; `recv` blocks until a message arrives. The 4 workers could then
    send their partial sums to a 5th task, which adds them (a reduce done inside the simulator).
- OpenMP threads are the opposite case: they share one address space, so they combine through
  shared variables (reduction, atomic, critical) and must avoid data races.

## Rules for a correct (and fast) parallel program
1. **Find the independent work first.** Iterations that write different elements
   (`c[i] = a[i] + b[i]`) can run in parallel. A loop-carried dependence (`a[i] = a[i-1] + x`)
   cannot as written; it needs another algorithm, such as a scan (02, [`practice/q06`](../practice/q06_prefix_sum.c)).
2. **No data races.** If two threads use the same variable and at least one writes it without
   synchronisation, the result is wrong and changes from run to run. A race can stay hidden
   at `-O2` or with few threads, so one correct run proves nothing (05).
3. **Private or shared.** Variables declared before the region are shared; variables declared
   inside it, and the loop index, are private. Temporaries must be private. `default(none)`
   makes you decide for each variable; `firstprivate` gives a start value, `lastprivate` the
   last value (07, 08, 18).
4. **Combine with a reduction, not a lock.** To fold the loop into one value (sum, count,
   product, max, min, `&&`, an array with `[:n]`), use `reduction`. For a value plus its
   index, keep a per-thread best and merge once per thread in `critical` (03, 04, 22).
5. **Synchronisation costs time.** reduction is cheaper than atomic, which is cheaper than
   critical. Never put a critical section in a hot loop when a local variable can collect
   the result. Every barrier costs too; `nowait` is allowed only when the next loop does not
   read what other threads wrote (05, 06, 09, 10).
6. **Balance the load.** Iterations of unequal cost need `schedule(dynamic, c)` or `guided`.
   A cyclic `static,1` balances only if the cost does not follow `i mod p`. On the simulator,
   a 5th equal task on 4 processors runs alone (12, example (e)).
7. **Choose the granularity.** Every parallel region, task, chunk or message has a fixed cost
   (microseconds in OpenMP, set-up instructions on the simulator). Parallelise the outer loop,
   give recursive tasks a cutoff, avoid tiny chunks, and keep small inputs serial. The
   opposite problem is too few pieces for the threads: `collapse` makes more (15 cutoff,
   13 collapse, example (a) set-up repeated by every task).
8. **Watch the memory.** Per-thread slots in one cache line slow down through false sharing;
   a loop that only streams memory (vector add) stops scaling once the memory bus is full (21, 02).
9. **Amdahl's law.** If a fraction f of the work stays serial, the speedup on p processors is
   at most 1 / (f + (1 - f) / p), and never more than 1 / f (f = 10%: at most 3.1 on 4
   cores, 10 on any number). Report S = T_serial / T_p and E = S / p, time only the
   parallel part, keep the best of several runs and state the core count (20, example (a)).
10. **Always check against the serial result.** Every example here prints `OK` only when it
    matches its serial version.

## Related material in this repo
- **The OpenMP labs:** [`lab7/`](../lab7/README.md) (vector add, sum, max/min, matrices),
  [`lab8/`](../lab8/README.md) (search, sort, histogram, transpose, convolution),
  [`lab9/`](../lab9/README.md) (merge sort with tasks, schedules, even/odd count).
  Syntax and report answers: [`CHEATSHEET.md`](../CHEATSHEET.md); method: [`GUIDE.md`](../GUIDE.md).
- **Practice programs q20–q29** (threads, data sharing, synchronisation, tasks):
  [`q20_hello_threads`](../practice/q20_hello_threads.c),
  [`q21_data_sharing`](../practice/q21_data_sharing.c),
  [`q22_sync_constructs`](../practice/q22_sync_constructs.c),
  [`q23_counter_race`](../practice/q23_counter_race.c),
  [`q24_sections`](../practice/q24_sections.c),
  [`q25_locks_histogram`](../practice/q25_locks_histogram.c),
  [`q26_schedule_runtime`](../practice/q26_schedule_runtime.c),
  [`q27_fibonacci_tasks`](../practice/q27_fibonacci_tasks.c),
  [`q28_quicksort_tasks`](../practice/q28_quicksort_tasks.c),
  [`q29_tasks_list_tree`](../practice/q29_tasks_list_tree.c).
  All of them: [`practice/README.md`](../practice/README.md).
- **Problems P001–P020** ([basics, synchronisation and performance](../PROBLEMS.md#basics-synchronisation-and-performance-effects)):
  [`p001`](../problems/p001_hello_threads.c) hello threads,
  [`p002`](../problems/p002_tsum_race.c) race and three fixes,
  [`p003`](../problems/p003_nested_parallel.c) nested regions,
  [`p004`](../problems/p004_reduction_identity.c) reduction start value,
  [`p005`](../problems/p005_lastprivate_search.c) lastprivate trap,
  [`p006`](../problems/p006_private_init.c) private start values,
  [`p007`](../problems/p007_master_vs_single.c) master vs single,
  [`p008`](../problems/p008_speedup_amdahl.c) speedup and Amdahl,
  [`p009`](../problems/p009_amdahl_serial_fraction.c) serial fraction,
  [`p010`](../problems/p010_sum_sync_methods.c) four ways to sum,
  [`p011`](../problems/p011_maxmin_without_reduction.c) max/min without reduction,
  [`p012`](../problems/p012_false_sharing.c) false sharing,
  [`p013`](../problems/p013_chunk_size_one.c) chunk size 1,
  [`p014`](../problems/p014_first_touch.c) first touch,
  [`p015`](../problems/p015_lock_deadlock.c) lock deadlock,
  [`p016`](../problems/p016_bank_transfers.c) lock per account,
  [`p017`](../problems/p017_producer_consumer.c) producer-consumer,
  [`p018`](../problems/p018_parallel_random.c) parallel random numbers,
  [`p019`](../problems/p019_serial_vs_one_thread.c) serial vs 1 thread,
  [`p020`](../problems/p020_simd_vectorise.c) simd.
- **Tasks and scheduling problems** in [`PROBLEMS.md`](../PROBLEMS.md#tasks-recursion-graphs-and-applications):
  tasks: [`p052`](../problems/p052_mergesort_parallel_merge.c) merge sort with a parallel merge,
  [`p068`](../problems/p068_block_matmul_tasks.c) recursive matmul,
  [`p087`](../problems/p087_smallest_factor_tasks.c) early stop,
  [`p089`](../problems/p089_task_dependencies.c) `depend`,
  [`p090`](../problems/p090_recursive_sum_tasks.c) recursive sum and max,
  [`p091`](../problems/p091_nqueens_tasks.c) N-queens,
  [`p092`](../problems/p092_adaptive_simpson_tasks.c) adaptive Simpson,
  [`p104`](../problems/p104_pipeline_sections_tasks.c) pipeline;
  schedules and load balance: [`p030`](../problems/p030_adaptive_integration.c),
  [`p060`](../problems/p060_triangle_sum.c), [`p069`](../problems/p069_triangular_matmul.c),
  [`p076`](../problems/p076_sparse_csr_spmv.c), [`p099`](../problems/p099_perfect_amicable.c);
  Lab 9: [`task1_mergesort.c`](../lab9/task1_mergesort.c), [`task2_schedule.c`](../lab9/task2_schedule.c).
- **OpenMP inside the simulator:** [`e22_openmp_parallel_processors`](../minicomputer/extensions/e22_openmp_parallel_processors/README.md)
  runs each round's time slices on real threads (`threadprivate`, `critical`), and
  final_practice Q096–Q100:
  [`q096`](../final_practice/q096_parallel_compile/README.md) compile the programs in parallel,
  [`q097`](../final_practice/q097_simd_vector_lanes/README.md) `omp simd` for the 8 vector lanes,
  [`q098`](../final_practice/q098_parallel_round_race/README.md) parallel round and its race,
  [`q099`](../final_practice/q099_count_instructions/README.md) counting with critical, atomic or per processor,
  [`q100`](../final_practice/q100_simulator_speed/README.md) simulator speed with 1 and 4 threads.
- **Cooperating tasks on the simulator:** [`q086`](../final_practice/q086_semaphores/README.md) semaphores,
  [`q087`](../final_practice/q087_shared_page/README.md) shared page,
  [`q088`](../final_practice/q088_message_passing/README.md) message passing;
  the scheduler itself: [`../minicomputer/README.md`](../minicomputer/README.md), [`lab6/`](../lab6/README.md).
