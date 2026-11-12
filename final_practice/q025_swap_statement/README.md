# Q025 — `swap x1 x2`: one line, three instructions

## Question
Add a statement `swap xA xB` that exchanges two registers without using a third one. It
must compile to 3 instructions. Branches to labels after a `swap` must still be
correct, and the Lab 6 `.freq` report must count the `swap` line once each time it runs
(not 3 times). Test it inside a loop and with a forward branch that jumps over a swap.

## Answer
**The 3 instructions** are `xA = xA + xB; xB = xA - xB; xA = xA - xB`. Starting from xA = a
and xB = b:
- after the add, xA = a + b;
- the first subtraction gives xB = (a + b) - b = a;
- the second gives xA = (a + b) - a = b.

The processor adds and subtracts in unsigned 32-bit arithmetic, so the swap is still
right when a + b overflows: the test swaps 2147483647 and 100. No new opcode is needed;
only the compiler and the `.freq` report change.

**One line becomes 3 instructions.** This is the general recipe:
- **Pass 1** counts how many instructions each line produces: `line_size()` returns 3
  for `swap` and 1 otherwise. Labels are instruction indices, so a label after a `swap`
  is 2 higher than `count++` would make it, and every branch offset is computed from it.
- **Pass 2** encodes the 3 instructions (`encode_swap()`) and pushes each one with
  `line_of = ln`, the same source line, so `index` advances by 3.
- **`.freq` report (`os.cpp`):** a line's count is the count of its *first* instruction
  only (`line_of[i-1] != line_of[i]`). Adding up all three would report 30 for a line
  that ran 10 times.

**Flags:** swap changes them, because it is made of add and sub, so do not put it
between a compare and its branch. `swap x1 x1` is rejected, because the trick gives 0
when both operands are the same register.

## Changes
| File | Function | Change |
|---|---|---|
| `compiler.cpp` | `line_size()` (new) | 3 for a `swap` line, otherwise 1 |
| `compiler.cpp` | `encode_swap()` (new) | checks for two different x registers; emits `ADD`, `SUB`, `SUB` |
| `compiler.cpp` | `compile_file`, pass 1 | `count += line_size(tokenize(s))` |
| `compiler.cpp` | `compile_file`, pass 2 | `ins[3][4]`; calls `encode_swap` or `encode_statement`; pushes `n` instructions, each with `line_of = ln` |
| `os.cpp` | `write_frequency_file` | adds `exec_count` only for the first instruction of each line |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the changes by hand (about 30 lines).

## Test
`./test.sh`: runs `swap_sort.txt`, which has three parts:
- it sorts the pair (9, 4), with `BLE .sorted` jumping over the swap (offset +4);
- a Fibonacci loop, `.fib ... swap x4 x5 ... BNE .fib`, gives F(10) = 55 and F(11) = 89;
- it swaps two values whose sum overflows.

The test also checks the `.freq` count (10, not 30), the 3 listing lines `01 04 04 05`,
`02 05 04 05` and `02 04 04 05`, the branch offsets, that `swap x3 x3` is rejected, and
that `sum_array` still works.

## Viva
- **What breaks if pass 1 still does `count++`?** Every label after a swap ends up 2 instructions too low. `BLE .sorted` then lands in the middle of the swap, and `BNE .fib` jumps back to `x5 = 1`, which restarts the loop forever until the endless-loop guard stops the task.
- **Why not use a temporary register (`x255 = xA; xA = xB; xB = x255`)?** That version keeps the flags and allows `swap x1 x1`, but it silently destroys x255, which the program may be using. The add/sub version needs no spare register; the price is the flags. An XOR swap would also work, but this machine has no XOR.
- **Why count only the first instruction in `.freq`?** All 3 instructions run exactly once each time the line runs, so any one of them gives the line's count, and the first is the natural choice. The `-c` listing still shows all 3, each with the `swap` source text.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `sum_array.txt`, `sum_array_data.byte`, `swap_sort.txt`, `swap_sort_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
