# Q083 — Instruction `sleep n`

## Question
Add a statement `sleep n` (n a constant 0..255) with its own opcode. When a task executes
it, its current time slice ends at once and the scheduler skips the task for the next n
rounds; after that it continues with the instruction after `sleep`. The other tasks keep
running. Give the encoding, and use `log.txt` to show that a task sleeping 2 rounds misses
exactly 2 rounds.

## Answer
- **Encoding `E0 00 00 n`.** `OP_SLEEP = 0xE0`, with the constant in the last byte, like
  `xD = c`. `encode_statement()` parses `sleep <constant>` next to `print`, and
  `disassemble()` prints `sleep n`.
- **Processor:**
  - `execute()` only records the request in `sleep_left[p] = n`. This is a per-processor
    array, like the registers, and `reset()` clears it.
  - `process_instructions()` leaves the slice loop right after a `sleep`
    (`if (opcode == OP_SLEEP) break;`), so the rest of the slice is given up.
- **Scheduler.** In the round loop, a task with `sleep_left[p] > 0` does not run; its
  counter goes down by one instead. The task stays in the ready queue and keeps its
  processor, which is idle meanwhile, so it wakes up in its old place.
- **Consequences:**
  - `sleep 0` only ends the current slice; the task runs again in the next round.
  - Sleeping is counted in rounds, not instructions or seconds. A skipped round runs
    nothing, so the task's instruction count does not grow.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_SLEEP = 0xE0` |
| `compiler.cpp` | `encode_statement` | `sleep n` → `E0 0 0 n`; n must be a constant |
| `compiler.cpp` | `disassemble` | `sleep 2` |
| `processor.h`, `processor.cpp` | globals, `reset` | `int sleep_left[NP]`, cleared in `reset()` |
| `processor.cpp` | `execute` | `case OP_SLEEP: sleep_left[p] = src2;` |
| `processor.cpp` | `process_instructions` | end the slice after a `sleep` |
| `os.cpp` | `scheduler` | `if (sleep_left[p] > 0) sleep_left[p]--; else process_instructions(...)` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` runs `sleeper.txt` (print 1, `sleep 2`, print 2) next to `counter.txt`. In
slices of 10, counter prints 2, 3, 2 and 3 values in rounds 1–4:

| Round | sleeper (pid 1) | counter (pid 2) prints |
|---|---|---|
| 1 | `x1 = 1`, print 1, `sleep 2` (3 instructions) | 1, 2 |
| 2 | skipped | 3, 4, 5 |
| 3 | skipped | 6, 7 |
| 4 | `x1 = 2`, print 2, store, halt | 8, 9, 10 |

- **`log.txt`** pids: `1,2,2,2,2,2,2,2,1,2,2,2`. The second print is line 9, right after
  counter's 7.
- **The trace** shows runs of `3x1,30x2,4x1,13x2`: counter runs 30 instructions in a row
  while the sleeper sleeps. The sleeper executes 7 instructions in all.
- **`sleep 0`** moves the second print to line 4, the first line of round 2.
- **The rest:** the listing shows `E0 00 00 02   sleep 2`, `sleep x1` is a compile error,
  and `sum_array` still gives 45.

## Viva
- **Why not a loop that waits?** That is busy waiting and burns every slice. A sleeping task executes nothing.
- **Why keep the counter per processor?** In lab6 a task owns its processor until it ends, so `p` identifies the task, exactly as for its registers. With context switching (extension e18) it would move into the PCB.
- **How does a real OS do it?** The task goes to a sleep queue and gives its processor to another task. A timer interrupt wakes it after a wall-clock time. Here the round counter is the clock, and the processor stays idle.
