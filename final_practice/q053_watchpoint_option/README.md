# Q053 — Watchpoint option `-w ADDR`

## Question
Add a command-line option `-w ADDR` (a data address). Every time a task writes that
address, print one line with the pid, the instruction (index and disassembly) and the
old and new value of the word. A vector store that covers ADDR counts too. Without `-w`
the output must not change.

## Answer
Every data write goes through `store_word(addr, value)` in `processor.cpp` (a vector
store calls it 8 times), so the watchpoint goes there. A word store writes the bytes
addr..addr+3; if ADDR is one of them, the function first reads the old word with
`load_word()` (the same bytes, so no extra page fault), then stores, then prints:

```
[watch] pid 1, instruction 3 ([8] = x2): [8] 7 -> 10
```

`watch_addr` is a global (−1 = off) set by `-w` in `main()`; `strtol(..., 0)` accepts
decimal or `0x` hex, and an address outside 0..DATA_SPACE−1 prints the usage.

## Changes
| File | Function | Change |
|---|---|---|
| `processor.h` | (declarations) | `extern int watch_addr;` |
| `processor.cpp` | (globals) | `int watch_addr = -1;` |
| `processor.cpp` | `store_word` | if the word covers ADDR: read the old value, store, print `[watch] ...` |
| `main.cpp` | `main`, `usage` | parse `-w ADDR`; usage text; `#include <cstdlib>` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 16 lines by hand.

## Test
`./test.sh`: `watch.txt` with `-w 8` writes [8] three times in a loop (7 → 10 → 20 → 30)
and once more with a vector store of 8 words from address 0 (30 → 30): 4 lines, in
order. `sum_array`, running beside it, never writes [8]. `-w 36` on sum_array shows the
sum replacing the last element (9 → 45), `-w 0x24` is the same address, `-w 4096` is
rejected, and without `-w` there is no `[watch]` line and the sum is still 45.

## Viva
- **Why in `store_word` and not in `execute()`?** It is the only path to data memory for every kind of store (`[xA] = xS`, `[c] = xS`, vector stores), so none is missed.
- **Why report a write of the same value?** The question asks for every write; a debugger's watchpoint also triggers on the write itself, not on a change.
- **Can reading the old value cause a page fault?** Only on an unmapped page, and then the store would map that same page anyway: the word read is the word about to be written.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `sum_array.txt`, `sum_array_data.byte`, `watch.txt`, `watch_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
