# Q057 — Memory footprint of each task

## Question
Report each task's data memory footprint when it ends: the highest data address it read
or wrote, and the number of distinct words it wrote (writing the same word 5 times
counts once). Test it with a program that writes one word several times and reads a far
address.

## Answer
Both numbers are collected where data memory is used, in `load_word()` and
`store_word()`, after a successful access:
- `data_top[p]`: the highest address passed to either function (−1 = no data access).
- `words_written[p]` with a table `written_at[NP][DATA_SPACE]`: the first store to an address sets its entry and adds 1; later stores to the same address add nothing. A vector store is 8 words.

All three are per processor and cleared in `reset()`; `finalize()` prints:

```
[footprint] pid 1 (footprint): highest data address 200, 3 distinct word(s) written
```

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern int data_top[NP], words_written[NP];` |
| `processor.cpp` | (globals), `reset` | the two counters and `written_at[NP][DATA_SPACE]`; clear them |
| `processor.cpp` | `load_word`, `store_word` | raise `data_top`; first store at an address: `words_written++` |
| `os.cpp` | `finalize` | print `[footprint] pid N (prog): highest data address A, W distinct word(s) written` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 13 lines by hand.

## Test
`./test.sh`: four tasks at once. `footprint.txt` writes [8] five times and [100] and [4]
once (3 words) and reads [200] (the highest). sum_array reads up to [36] and writes only
[36] (1 word). array_add's two vector stores write C[0..15] (16 words), up to
132 + 60 = 192. pages stores at 1020, 2040, 3825 and 0 (4 words, highest 3825).
sum_array still stores 45.

## Viva
- **Why a table and not a count of stores?** A count would add the 5 stores to [8] five times; "distinct" needs to remember which words were already written.
- **How big is `written_at`?** One `bool` per data byte address per processor: 4 × 4096 bytes. If every access is aligned, `DATA_SPACE / 4` entries indexed by `addr / 4` are enough.
- **Is the highest address what sizes the `.out` file?** No: `data_high` follows writes only (+4 bytes) and sizes the output; a read of [200] raises the footprint but not the file.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `footprint.txt`, `footprint_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
