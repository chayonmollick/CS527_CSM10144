# Q056 — Decimal dump of the final data memory

## Question
When a task ends, the simulator writes its data memory to `<data>_<pid>.out` in hex bytes,
which is hard to read. Also write `<program>_<pid>.dump` with the same memory as 32-bit
words in decimal, one per line with its address, for example `[36] = 45`. Negative
numbers must appear as negative.

## Answer
`finalize()` already builds the final data image `out` (the task's data pages read
through its page table: `size` bytes, a multiple of 4) to write the `.out` file. The dump
goes right after that: one line per word, `get_word(out, a)` for a = 0, 4, 8, ... It
returns the little-endian word as a signed `int`, so `FF FF FF FF` prints as −1.

```
[0] = 9
[4] = 1
...
[36] = 45
```

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `finalize` | after `write_byte_file(out_name, out)`: write `<program>_<pid>.dump`, one `[a] = get_word(out, a)` per word |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 6 lines by hand.

## Test
`./test.sh`: sum_array's dump has 10 lines, from `[0] = 9` to `[36] = 45`, and every line
equals the word at that address in `sum_array_data_1.out`. sum_n's dump shows `[8] = -1`
and the sum `[24] = 99`. pages stores a word at 3825, so its dump has 3832 / 4 = 958
lines, and that unaligned 7 appears as 7 × 256 = 1792 in the word at 3824. sum_array
still stores 45.

## Viva
- **How many lines does the dump have?** As many words as the `.out` file: the larger of the data file and the highest byte written, rounded up to a multiple of 4.
- **Why does `[3824]` show 1792 for pages?** The program stores 7 at the unaligned address 3825; byte 3825 is the second byte of the word at 3824, so that word is 7 × 256.
- **Why use `out` instead of reading `memory[]` again?** `out` is already translated through the page table (unmapped pages read as 0), so the `.out` and `.dump` files always agree.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `sum_n.txt`, `sum_n_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
