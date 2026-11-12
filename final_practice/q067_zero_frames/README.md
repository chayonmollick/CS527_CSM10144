# Q067 — `ZERO_FRAMES`: clearing demand-mapped frames

## Question
On the first touch of a data page, `getPhysicalAddress()` gives the page a zeroed frame.
Add a switch `ZERO_FRAMES` to `config.h` (default 1). With 0, the frame is mapped
without being cleared. Show with two tasks that run one after the other that a task can
then read data left behind by a task that has finished. Explain why this is a security
problem, and why the pages filled by the loader do not have it.

## Answer
`finalize()` frees a frame by setting `freePages[frame] = 0`. It never clears the bytes,
so a freed frame still holds its old contents. The only thing that hides them is the
`memset` when the frame is handed out again. The switch makes that `memset` conditional:
```c
if (ZERO_FRAMES) std::memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
```
The `#define` is wrapped in `#ifndef ZERO_FRAMES`, so the switch can be set without
editing the file: `make clean && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DZERO_FRAMES=0"`.

**The leak.** Run `secret`, `sum_array`, `array_add`, `counter`, `snoop` (pids 1-5) with `-b`:
1. Pids 1-4 take the 4 processors; `snoop` (pid 5) waits.
2. `secret` gets frames 1-2, stores 9999 at 512 (data page 1 → frame 9), and ends in
   round 1. Frames 1, 2 and 9 become free, but frame 9 still holds 9999.
3. In round 2, `snoop` loads on frames 1-2. It reads 512 without writing it first. Data
   page 1 is demand-mapped to the lowest free frame, which is 9.
4. With `ZERO_FRAMES 0`, `snoop` reads **9999**, the other task's data. With 1 it reads 0.

**Why it is a security problem.** A frame is memory that one process owned. Passwords,
keys or another user's file contents stay in it after that process ends. Giving the frame
to another process without clearing it breaks isolation: it is an information leak
between processes (the "object reuse" / residual-information rule of OS security). It
does not affect correct programs, which write before they read; that is why `sum_array`
still gives 45. A buggy or malicious program reads before writing.

**Why the loader is safe.** `copy_in()` clears the whole frame and then copies the
program or data file into it. Even the tail of the last page, after the end of the file,
is 0. This switch only controls the demand-mapping path.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | — | `#ifndef ZERO_FRAMES` / `#define ZERO_FRAMES 1` / `#endif` |
| `os.cpp` | `getPhysicalAddress` | `memset` of the new frame only `if (ZERO_FRAMES)` |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` builds the default, runs the 5 tasks and checks:
- that `snoop` waited, that `secret` wrote 9999 at 512, and that `snoop` read 0;
- that `sum_array` alone gives 45.

It then rebuilds with `-DZERO_FRAMES=0` (no warnings), runs the same 5 tasks, and checks
that `snoop` now saves 9999 and that `sum_array` still gives 45.

## Viva
- **Clear on free or on allocation?** Either closes the leak. Clearing on allocation, as here, does the work only for frames that are reused. Linux clears an anonymous page when it is first faulted in; Windows keeps a pool of pre-zeroed pages filled by a low-priority thread.
- **Is the `memset` expensive?** 512 bytes per page fault, which is tiny next to the fault itself (on a real system a fault can also mean a disk read). It is never a good reason to skip it.
- **Why exactly frame 9?** `getFreePage()` returns the lowest free frame. When `snoop` faults, frames 1-8 are taken (`snoop` has 1-2, pids 2-4 have 3-8) and 9 is the first free one.
- **Could the leak also happen with the registers?** No. The loader calls `reset(p)`, which clears the registers, flags and PC of the processor before a new task starts. The same rule applies to every resource a task inherits.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `counter.txt`, `counter_data.byte`, `secret.txt`, `secret_data.byte`, `snoop.txt`, `snoop_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
