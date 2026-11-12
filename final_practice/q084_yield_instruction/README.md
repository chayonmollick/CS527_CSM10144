# Q084 — Instruction `yield`

## Question
Add a statement `yield`, with no operands and its own opcode, that ends the current time
slice at once. The task gives up the rest of its slice and continues with the next
instruction in its next turn. Give the encoding, and show, next to a normal counter, a loop
that yields after every print.

## Answer
- **Encoding `E1 00 00 00`.** `OP_YIELD = 0xE1`. The compiler accepts `yield` alone on its
  line, like `halt` in Q000, and `disassemble()` prints `yield`.
- **Processor:**
  - `execute()` has nothing to compute, but it still needs `case OP_YIELD: break;`.
    Without it the opcode is an "unknown opcode" error.
  - The work is in `process_instructions()`: after a `yield` it leaves the slice loop
    (`if (opcode == OP_YIELD) break;`).
  - The PC already points past the `yield`, so the next turn continues after it.
- **The scheduler is unchanged.** The task goes to the end of the ready queue as after any
  slice.
- **Counting.** `yield` is an executed instruction, so it counts in the task's
  instructions and in the `.freq` report.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_YIELD = 0xE1` |
| `compiler.cpp` | `encode_statement` | `yield` → `E1 00 00 00` |
| `compiler.cpp` | `disassemble` | `yield` |
| `processor.cpp` | `execute` | `case OP_YIELD: break;` |
| `processor.cpp` | `process_instructions` | `if (opcode == OP_YIELD) break;` after `execute()` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` runs `polite.txt` next to `counter.txt`. `polite.txt` counts to 5, and every
loop turn is add, print, yield, sub, BNE.
- **polite's slices**, measured with the `-t` trace:
  - 4 instructions: `x1 = 0`, add, print, yield;
  - then 5, 5, 5, 5: sub, BNE, add, print, yield;
  - then 4: sub, BNE, store, halt. That is 28 instructions in all.
- **counter's slices** are unchanged: 10, 10, 10, 10, 3.
- **`log.txt`:** polite prints once per round, counter 2, 3, 2, 3 times:
  `1,2,2,1,2,2,2,1,2,2,1,2,2,2,1`. Counter ends in round 5 and polite in round 6.
- **The rest:** the results are 5 and 10, the listing shows `E1 00 00 00   yield`, and
  `sum_array` still gives 45.

## Viva
- **What is yield for?** A task with nothing useful to do, for example one polling a flag, gives its slice away instead of spinning. That is cooperative multitasking, like `sched_yield()` in POSIX.
- **Does anyone gain from it in lab6?** Not directly. Each task has its own processor, so the given-up instructions go to nobody, and the processor idles until the next round. Yield pays off when tasks share a processor (context switching, extension e18).
- **`yield`, `sleep 0` and `halt`:** `halt` ends the task, `yield` ends only the slice, and `sleep n` (Q083) ends the slice and also skips the next n rounds.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_data.byte`, `polite.txt`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
