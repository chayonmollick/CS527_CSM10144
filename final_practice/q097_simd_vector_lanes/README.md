# Q097 — Execute the 8 lanes of a vector instruction with `omp simd`

## Question
A vector instruction (`v3 = v1 + v2`, `v3 = v1 * 5`, `v3 = v1 - x2`) works on 8 lanes
of 32 bits, but `vector_op()` handles them in a plain loop, one lane at a time. Use
`#pragma omp simd` so that the compiler executes several lanes per machine instruction.
Show that every vector result stays identical. Explain the difference between SIMD and
threads, and why one thread per lane (`#pragma omp parallel for` over the 8 lanes)
would be slower.

## Answer
**Construct:** `#pragma omp simd` on the lane loop of `vector_op()`. It tells the compiler
that the 8 iterations are independent, so it may run them in vector registers. ARM NEON
and x86 SSE hold four 32-bit lanes, so 8 lanes take 2 vector instructions; AVX2 needs one.

**Why it is correct:** lane `i` reads only `va[i]` and `b[i]` and writes only `vd[i]`.
The destination may also be an operand (`v1 = v1 * v1`), but every lane reads its own
element before writing it, and no lane reads another lane's element, so there is no
dependence between iterations. And it is all one thread, so no data race is possible.

**The pragma alone was not enough.** On the original loop GCC reports
`not vectorized: unsupported control flow in loop`. The cause is the second operand: a
vector lane, a constant or an x register, picked by a branch inside the loop. So the
patch:
1. first fills `b[8]`, either with `memcpy` from the vector register or with the constant or x value copied into all 8 lanes (a *broadcast*);
2. takes the register pointers before the loop, because `dest` and `src1` are globals;
3. then every lane runs the same branch-free code, and GCC reports `loop vectorized using 16 byte vectors`.

Without the pragma, the same clean loop at `-O2` gives
`missed: would need a runtime alias check`, because `vd` might overlap `va` or `b`. At
`-O3`, GCC vectorizes it behind a run-time overlap test. The pragma is the programmer's
promise that replaces that test.

**SIMD vs threads:**
- **SIMD** is data parallelism inside one core: one instruction, several lanes, no synchronisation. `OMP_NUM_THREADS` changes nothing, as the timings show.
- **Threads** are several cores, each with its own instruction stream. Every parallel region pays a fork/join, and the data has to move between the cores' caches.

**Why one thread per lane is slower:**
- The 8 lane operations take a few nanoseconds. A parallel region costs about 13 µs here (libgomp on macOS; about 7 µs with LLVM libomp; typically 1–3 µs on Linux), so the overhead is about 1000× the work.
- The 8 results are 32 bytes in one cache line, so 4 cores write to the same line (false sharing).
- If the rounds already run in parallel (Q098), a nested region gets only 1 thread by default.

Measured: 0.181 s against 0.035 s for the same programs.

Vector loads and stores keep their plain loops. Each lane is 4 byte accesses through the
MMU and can stop the task on a fault (a `return` inside the loop), which a simd loop may
not contain.

## Changes
| File | Function | Change |
|---|---|---|
| `processor.cpp` | `vector_op()` | second operand of each lane first (`memcpy` of the vector register, or the constant/x value copied to all 8 lanes); register pointers taken before the loop; `#pragma omp simd` on the lane loop |
| `processor.cpp` | `vector_op()` | `#ifdef Q097_LANE_THREADS`: `#pragma omp parallel for` instead, only for the timing experiment |
| `Makefile` | | OpenMP as in e22: `g++ -fopenmp`; on macOS Homebrew `g++-<N>`, else clang++ + libomp |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`. To build the threads-per-lane version:
`make clean && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DQ097_LANE_THREADS"`.

## Test
`./test.sh` builds three versions: lab6 unmodified (with the same compiler), the simd
version, and the threads-per-lane version. Each runs four programs:
- `vec_ops.txt`: all 9 vector add/sub/mul forms, `v1 = v1 * v1`, wrap-around past 2^31;
- `fir.txt` and `array_add.txt`;
- `vec_heavy.txt`: 2000 iterations of 6 vector + 3 scalar instructions.

All four `.out` files must equal lab6's byte for byte. The test also prints GCC's
vectorizer report for the simd loop. The base tests pass 33 of 33 with 1 and 4 threads.

Timings are machine-specific: Apple M4, macOS, Homebrew GCC 15, with other jobs running.
```
  ok    builds without warnings
  ok    simd, 1 thread: all 4 tasks' data identical to lab6
  ok    simd, 4 threads: identical to lab6
  ok    threads per lane, 4 threads: identical to lab6
  ok    vec_ops: lane 0 of v1 + v2 = 1 + 10
  ok    vec_ops: lane 7 of v1 + 255 wraps around
  ok    vec_ops: lane 1 of v1 * x1 = -2 * 7
  ok    vec_ops: v1 = v1 * v1 (dest = both operands), lane 1 = 4
  ok    fir out[15] = 744, array_add C[15] = 131
  time  (machine-specific, not checked; 12000 of the 18000 instructions of vec_heavy are vector)
        lab6, plain lane loop       : 0.035 s
        omp simd, 1 thread          : 0.035 s
        omp simd, 4 threads         : 0.036 s   (simd is inside one thread)
        threads per lane, 4 threads : 0.181 s   (a parallel region per vector instruction)
  info  vectorizer, simd loop (pragma at line 157): processor.cpp:160:36: optimized: loop vectorized using 16 byte vectors
  ok    the old programs still work (sum_array = 45)

10 passed, 0 failed
```
SIMD gains no visible time in the simulator. A vector instruction spends most of its
time on the fetch (4 MMU translations), the `switch` and the counters, and every slice
sleeps about 14 µs. The lane loop itself is a few nanoseconds.

## Viva
- **How do you know the loop was vectorized?** Ask the compiler: GCC `-fopt-info-vec-optimized` (the test does), clang `-Rpass=loop-vectorize`. Or look for vector instructions in the assembly (`-S`).
- **What does `omp simd` promise, and what would break it?** That no iteration depends on another. A loop like `vd[i] = vd[i-1] + va[i]` (a prefix sum) would give wrong results under the pragma.
- **Why does `OMP_NUM_THREADS` not matter here?** `simd` creates no threads. It only changes which machine instructions one thread executes. (`-fopenmp-simd` enables just the simd pragmas, without the threading runtime.)
- **Why was the original loop not vectorized?** The second operand came from three places chosen inside the loop (a conditional load). Building the 8-lane `b[]` first makes every lane do exactly the same thing.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `fir.txt`, `fir_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `vec_heavy.txt`, `vec_ops.txt`, `vec_ops_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
