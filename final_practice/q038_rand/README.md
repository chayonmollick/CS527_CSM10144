# Q038 — Pseudo-random numbers `x1 = rand`

## Question
Add an instruction `x1 = rand` that puts a pseudo-random number 0..32767 in x1. It must be
deterministic, so results can be checked, and every task must have its own generator: a
linear congruential generator (LCG) seeded with the task's pid and reset whenever a task is
loaded onto a processor. Tasks running at the same time must not change each other's
numbers. Give the encoding and the formula, and show it with several tasks.

## Answer
- **Encoding `8F D 0 0`:** this is MOVC (`0F D 0 c`) + 0x80. It fills a register like
  `xD = c`, but the value comes from the generator.
- **Generator:** the C standard's example `rand()`:
  - `state = state × 1103515245 + 12345`, computed in `unsigned` so it wraps modulo 2³²;
  - the result is bits 30..16 of the new state, `(state >> 16) & 0x7FFF`, which is 0..32767.
- **State per task:**
  - `rand_state[NP]` is processor state, like the registers and flags since Lab 4.
  - A task stays on its processor until it ends, so per-processor state is per-task state.
  - `loader()` sets `rand_state[p] = pid` right after it assigns processor p to the task.
  - When pid 5 later reuses processor 0, its generator therefore restarts from 5.
- **Deterministic:** pid 1 always gets 16838, 5758, 10113, … That is exactly what C's sample
  `rand()` gives after `srand(1)`. It does not depend on the other tasks or on how the
  scheduler interleaves them.
- **Flags:** unchanged. `rand` is a data move, not an add or a subtract.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | `enum Opcode` | `OP_RAND = 0x8F`: `xD = rand` |
| `processor.h` / `processor.cpp` | globals | `unsigned rand_state[NP]` |
| `processor.cpp` | `execute` | `case OP_RAND`: one LCG step, then `xD` = bits 30..16 |
| `os.cpp` | `loader` | `rand_state[p] = pid` after `current_pid[p] = pid` |
| `compiler.cpp` | `encode_statement` | `xD = rand` → `8F D 0 0` |
| `compiler.cpp` | `disassemble` | `x3 = rand` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` builds lab6 + the patch and runs 5 copies of `rnd.txt` at once. Each copy draws 6
numbers over 4 time slices, so the draws of different tasks interleave. It checks:
- pids 1, 2 and 4 get the sequences of seeds 1, 2 and 4;
- pid 5 had to wait. When it was loaded on processor 0 (after pid 1), it started from
  seed 5 and did not continue pid 1's sequence;
- `rnd.txt` run alone as pid 1 gives the same 6 numbers again;
- the encoding `8F 03 00 00`;
- that `sum_array` still works.

The expected numbers were computed separately with the same formula.

## Viva
- **Why seed in `loader()` and not in `reset()`?** `loader()` calls `reset(p)` *before* it sets `current_pid[p] = pid`, so `reset()` does not know the new pid yet. The seed belongs to the task being loaded.
- **Why not one global generator?** pid 1's numbers would then depend on how many numbers the other tasks drew in their time slices, so they would change with NP, the time slice or the task mix. Per-task state makes every task reproducible on its own.
- **Why return the high bits?** In an LCG modulo 2³², bit k repeats with period 2^(k+1), so bit 0 just alternates. The upper bits are far better. With c odd and a − 1 divisible by 4, the state goes through all 2³² values before it repeats.
- **How do you get a dice roll 1..6?** There is no `mod`, so compute r − (r / 6)·6 + 1: `x4 = x3 / 6`, `x4 = x4 * 6`, `x4 = x3 - x4`, `x4 = x4 + 1`.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `rnd.txt`, `rnd_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
