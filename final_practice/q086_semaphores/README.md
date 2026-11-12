# Q086 — Semaphores: `wait s` and `signal s`

## Question
Add semaphores to the language. `wait s` and `signal s` work on a global semaphore
s = 0..7 that all tasks share. Every semaphore starts at 1 unless a program sets it with
`sem s n`. `wait` takes 1 from a positive semaphore; at 0 the task blocks and skips its
time slices until another task signals. `signal` wakes the task that has waited longest,
or adds 1 if nobody waits. Show two tasks that take turns printing (1, 101, 2, 102, …).

## Answer
- **Three instructions:** `wait s` = `E8 s 0 0`, `signal s` = `E9 s 0 0` and
  `sem s n` = `EA s 0 n`. The semaphore number goes in the dest byte. They are statements
  with a keyword, like `print`, so `encode_statement()` parses them and checks s ≤ 7.
- **State, in the processor:**
  - `sem_value[8]` holds the values, all starting at 1.
  - `sem_queue[8]` holds the blocked processors of each semaphore in FIFO order.
  - `blocked[NP]` marks a processor whose task is blocked; `reset()` clears it when a task is loaded.
  - In lab6 a task keeps its processor until it finishes, so the processor number identifies the task.
- **`wait`:** if the value is > 0, take 1. Otherwise put p at the end of the semaphore's
  queue and set `blocked[p] = 1`.
- **`signal`:** if the queue is empty, add 1. Otherwise clear `blocked` of the first
  processor in the queue. The value stays 0 because the woken task uses up this signal.
  It continues after its `wait`, since the PC already points past it.
- **Skipping slices:** `process_instructions()` stops as soon as `blocked[p]` is set and
  runs nothing while it stays set. The scheduler still gives the task its turn every
  round, but it executes 0 instructions. There is no context switch in lab6, so **a
  blocked task keeps its processor, which stays idle**. A task that is still waiting
  for a processor therefore cannot run to signal anyone: with more than NP tasks, or
  with a `wait` that nobody will signal, the tasks deadlock and the scheduler never
  ends. The tests use at most 3 tasks.
- **Demo:** ping (pid 1) runs `sem 1 0` before pong executes anything, because pid 1's
  slice comes first in round 1. Semaphore 0 then means "ping may print" (1) and
  semaphore 1 "pong may print" (0). Round 1:
  1. Ping prints 1 and signals 1. It loops back and blocks in `wait 0` after 9 instructions.
  2. Pong passes `wait 1`, prints 101 and signals 0, which wakes ping. It then blocks in `wait 1`.

  Every later round repeats this, so `log.txt` alternates. Ping executes exactly 34
  instructions and pong 33, the counts of a straight run through each program. A blocked
  task spends nothing, unlike a busy-wait loop.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | defines, `enum Opcode` | `NUM_SEMS 8`; `OP_WAIT 0xE8`, `OP_SIGNAL 0xE9`, `OP_SEMSET 0xEA` |
| `compiler.cpp` | `encode_statement` | parse `wait s`, `signal s` and `sem s n` (s a constant 0..7, n 0..255) |
| `compiler.cpp` | `disassemble` | listing text for the three instructions |
| `processor.cpp` | globals, `reset` | `sem_value[8] = {1,…}`, `sem_queue[8]` (deque of processors), `blocked[NP]`; `reset` clears `blocked[p]` |
| `processor.cpp` | `bad_sem` | new: error for a semaphore number > 7 in a `.byte` program |
| `processor.cpp` | `execute` | `sem`: set the value. `wait`: take 1, or queue p and block. `signal`: wake the first in the queue, or add 1 |
| `processor.cpp` | `process_instructions` | the slice loop also stops while `blocked[p]` |

## Test
`./test.sh`:
- **Listing:** `sem 1 0` = `EA 01 00 00`, `wait 0` = `E8 00 00 00` and `signal 1` = `E9 01 00 00`. `wait 8` is a compile error.
- **`ping.txt` + `pong.txt`:**
  - `log.txt` shows pids `1 2 1 2 …` with values 1 101 2 102 … 5 105.
  - The outputs are 5 and 105.
  - The instruction counts are exactly 34 and 33.
- **Three copies of `mutex.txt`:** each prints 1, 2, 3 inside `wait 0` … `signal 0`, where semaphore 0 keeps its default 1.
  - `log.txt` shows `1 1 1 2 2 2 3 3 3`: the blocked copies are woken in FIFO order.
  - Each copy executed `wait 0` once.
  - Plain lab6 without the two lines prints `1 1 2 2 3 3 1 2 3`, because the first slice ends inside the loop.
- **Old programs:** `sum_array` still gives 45.

## Viva
- **Why is `wait` atomic here?** The simulator runs one instruction at a time, so nothing can come between the test and the decrement. A real kernel needs an atomic instruction (test-and-set, compare-and-swap) or must disable interrupts.
- **Blocking or busy waiting?** A spin loop on a flag burns every slice. A blocked task here executes 0 instructions, but it still holds its processor. To free the processor, the OS must save the registers in a PCB and load another task: a context switch (extension e18).
- **Why does `signal` not add 1 when it wakes a task?** The woken task completes its `wait` with this signal. Adding 1 as well would let a second task through the same `wait`.
- **How can it deadlock?** Take two tasks that each `wait` on a semaphore only the other one signals. Or let the task that should signal sit in the waiting queue while blocked tasks hold all 4 processors. Detection: every ready task is blocked and the shell is closed.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `mutex.txt`, `ping.txt`, `pong.txt`, `sem_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
