# Q096 — Compile the command-line programs in parallel

## Question
`./sim -b p1.txt d1.byte p2.txt d2.byte ...` compiles and loads the programs one after
another before the scheduler starts. Use OpenMP to compile all the programs given on the
command line (and read their data files) at the same time, before any of them is loaded.
Loading and scheduling stay serial. Which data is shared during the parallel part, and
which work must stay serial (think of printing, the pids and `program.byte`)? The output
must be exactly what the serial version prints.

## Answer
**Construct:** `create_task()` is split in two:
- `prepare()` compiles the program (or loads a `.byte` file), reads the data file and checks the sizes. It writes only its own `Prepared` slot.
- `commit()` does everything that touches shared state.

A new `create_tasks()` runs `prepare()` for all command-line pairs in
`#pragma omp parallel for schedule(dynamic, 1)`, then calls `commit()` serially in
command-line order. The schedule is dynamic because programs differ in length. The shell
still creates one task at a time with `create_task()`, which is `prepare()` + `commit()`.

**Why the parallel part is race-free:**
- `list` is sized before the loop and never resized, and iteration `k` writes only `list[k]`.
- `compile_file()` uses only local variables (the stream, the label map, the `Program` it fills) and the constant `COND_NAMES` table.
- The source and data files are only read, so several threads may read the same file.
- Nothing is printed inside the loop.

**Shared, so serial (in `commit()`, in command-line order):**
- **The pid** (`tasks.size() + 1`): pids follow the command line, and a program that fails gets none.
- **`program.byte`**: one file name for every task. The serial version leaves the *last source program* in it. In parallel, its content would depend on which thread wrote last, or two writes could mix.
- **Printing**: `printf` is thread-safe per call, but the order would be random. The test checks that the messages and the compile error appear in command-line order.
- **The process table, waiting queue and loader**: `tasks.push_back` can reallocate, and the loader changes `freePages`, `memory` and `proc_owner`.

The compile time is printed on **stderr**:
`[os] N program(s) compiled in X ms with T thread(s)`. That keeps stdout identical to
lab6.

## Changes
| File | Function | Change |
|---|---|---|
| `os.h` | | `#include <vector>`; declare `create_tasks()` |
| `os.cpp` | `prepare()` (new) | first half of the old `create_task`: compile or load, read data, size checks; fills its own `Prepared`; no printing, no `program.byte` |
| `os.cpp` | `commit()` (new) | second half: process-table check, `program.byte`, pid, messages, process table, waiting queue, loader |
| `os.cpp` | `create_task()` | `prepare()` + `commit()` (used by the shell) |
| `os.cpp` | `create_tasks()` (new) | `#pragma omp parallel for schedule(dynamic, 1)` over `prepare()`, time on stderr, then `commit()` in order |
| `main.cpp` | `main()` | `create_tasks(argv pairs)` replaces the `create_task` loop |
| `Makefile` | | OpenMP as in e22: `g++ -fopenmp`; on macOS Homebrew `g++-<N>`, else clang++ + libomp |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`.

## Test
`./test.sh` builds the unmodified lab6 as a reference. It runs one command line with
8 programs, with 1 and 4 threads:
- `broken.txt`, which has a compile error, sits in the middle;
- the last program is a precompiled `.byte` file, so `program.byte` must hold
  `complex_mul`, the last *source* program.

stdout, `program.byte`, all 14 `.out`/`.freq` files and `log.txt` must be byte-for-byte
equal to lab6's. Then it compiles 40 generated programs of 252 statements each with 1
and 4 threads. The base tests (`SIM=$PWD/sim ../minicomputer/tests/run_tests.sh`) pass
33 of 33 with 1 and with 4 threads.

Timings are machine-specific: Apple M4 (4 performance + 6 efficiency cores), macOS,
Homebrew GCC 15, with other jobs running.
```
  ok    builds without warnings
  ok    1 thread(s): exit code 1 (one program does not compile), as lab6
  ok    1 thread(s): output, program.byte, .out, .freq, log.txt same as lab6
  time  [os] 8 program(s) compiled in 0.375 ms with 1 thread(s)
  ok    4 thread(s): exit code 1 (one program does not compile), as lab6
  ok    4 thread(s): output, program.byte, .out, .freq, log.txt same as lab6
  time  [os] 8 program(s) compiled in 0.238 ms with 4 thread(s)
  ok    4 threads: messages in command-line order, the error in its place
  ok    the tasks after it keep their pids (sum_array is pid 3)
  ok    program.byte = the last source program (complex_mul)
  ok    results: fir 744, array_add 131, sum.byte 45
  time  [os] 40 program(s) compiled in 5.031 ms with 1 thread(s) (machine-specific, not checked)
  ok    1 thread(s): 40 big tasks finished, the last stored 250
  time  [os] 40 program(s) compiled in 1.790 ms with 4 thread(s) (machine-specific, not checked)
  ok    4 thread(s): 40 big tasks finished, the last stored 250
  ok    the old programs still work (sum_array = 45)

12 passed, 0 failed
```
With the 40 large programs, compiling is 2.8× faster on 4 threads. The 8 small examples
gain only 1.6×: each takes about 50 µs to compile, and the parallel region alone costs
about 13 µs here (libgomp on macOS).

## Viva
- **Why not write `program.byte` inside the loop under `#pragma omp critical`?** A critical section stops two threads from writing at the same time, but not the *order*. The last writer would change from run to run, while the serial version defines it: the last source program on the command line.
- **Why must the pids be given out serially?** Otherwise a pid would depend on which thread finished first. Failed programs get no pid, so the numbering needs the order.
- **Does the whole run get faster?** Hardly. Compiling takes milliseconds, while running the tasks takes far longer (every slice sleeps). By Amdahl's law, speeding up 1% of the run saves at most 1%.
- **When would `compile_file()` not be thread-safe?** If it kept state in `static` or global variables, such as a static label map, line counter or error buffer. Then it would need `threadprivate` copies or a lock.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `broken.txt`, `complex_mul.txt`, `complex_mul_data.byte`, `counter.txt`, `counter_data.byte`, `det3x3.txt`, `det3x3_data.byte`, `fir.txt`, `fir_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
