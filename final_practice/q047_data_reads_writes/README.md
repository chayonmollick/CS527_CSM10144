# Q047 — Count data memory reads and writes per task

## Question
Count, for every task, how many 32-bit data words it reads from memory and how many it
writes. A vector load or store moves 8 words, so it counts 8; instruction fetches are not
data accesses. Print both numbers when the task ends, and check them by hand for
`sum_array` and `array_add` running at the same time.

## Answer
Every data access goes through two functions in `processor.cpp`: `load_word()` (a scalar
load, or one of the 8 calls of a vector load) and `store_word()` (likewise for stores).
Counting there, after the access succeeded, gives 1 per word and 8 per vector access
without touching `execute()`. Instruction fetches call `read_byte()` directly, so they are
not counted. The two counters are per processor (`[NP]`) like all processor state, set to
0 in `reset()`, and printed by `finalize()`:

```
[mem] pid 1 (sum_array): 10 data word(s) read, 1 written
[mem] pid 2 (array_add): 33 data word(s) read, 16 written
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern long data_reads[NP], data_writes[NP];` |
| `processor.cpp` | (globals), `reset` | define them; set both to 0 |
| `processor.cpp` | `load_word` | `data_reads[proc_id]++` after a successful read |
| `processor.cpp` | `store_word` | `data_writes[proc_id]++` after a successful write |
| `os.cpp` | `finalize` | print `[mem] pid N (prog): R data word(s) read, W written` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 7 lines by hand.

## Test
`./test.sh`: runs `sum_array`, `array_add` and `pages` together. sum_array reads the size
and 9 elements (10) and writes the sum (1). array_add reads N (1) plus 4 vector loads
(32) = 33 and writes 2 vector stores (16). pages reads 3 words and writes 4. Then checks
that `sum_array` still stores 45.

## Viva
- **Why count in `load_word`/`store_word` and not in `execute()`?** They are the only way to data memory, so no load or store kind can be missed, and a vector access is counted per word automatically.
- **Why after the access, not before?** A word that faults (address outside the data space) is never transferred; the task stops with an error instead.
- **A word is 4 bytes and `getPhysicalAddress()` is called per byte. Why count 1?** The processor transfers one 32-bit word; translating byte by byte only handles a word that crosses a page boundary.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
