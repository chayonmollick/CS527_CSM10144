# Q059 — Simple timing model: cycles per task

## Question
Add a simple timing model: every instruction takes 1 cycle, every 32-bit data word read
or written costs 2 more cycles (a vector access moves 8 words), and every page fault
costs 100 more. Define the costs in `config.h`. When a task ends, report its cycles and
how they add up.

## Answer
Two of the three counts already exist per task: instructions (`steps[p]`, saved in
`t.steps`) and page faults (`page_faults_on[p]`, saved in `t.page_faults`). Only the data
words are new: `mem_words[p]`, increased in `load_word()` and `store_word()` (8 calls for
a vector access) and cleared in `reset()`. `finalize()` combines them with the costs
from `config.h`:

cycles = 1 × instructions + 2 × memory words + 100 × page faults

```
[time] pid 3 (pages): 329 cycles = 1 x 15 instructions + 2 x 7 memory words + 100 x 3 faults
```

The line says "faults", not "page faults": the base regression test counts the lines that
contain "3 page fault" to check the `[os]` line.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | (defines) | `CYCLES_PER_INSTRUCTION 1`, `CYCLES_PER_MEM_WORD 2`, `CYCLES_PER_PAGE_FAULT 100` |
| `processor.h` | (declarations) | `extern long mem_words[NP];` |
| `processor.cpp` | (globals), `reset` | define it; set to 0 |
| `processor.cpp` | `load_word`, `store_word` | `mem_words[proc_id]++` |
| `os.cpp` | `finalize` | compute the cycles; print `[time] ...` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of
`lab6` with `patch -p1 < solution.patch`, or type the 14 lines by hand.

## Test
`./test.sh`: sum_array: 71 + 2 × 11 = 93 cycles. array_add: 31 + 2 × (33 + 16) = 129.
pages: 15 instructions, 7 words and 3 page faults (data pages 1, 3 and 7 are touched for
the first time): 15 + 14 + 300 = 329. sum_array still stores 45.

## Viva
- **Why is the instruction fetch not a memory access here?** The 1 cycle per instruction includes it; only data words cost extra.
- **Which accesses cause a page fault?** The first touch of a data page the loader did not map (it maps only the pages of the data file); `getPhysicalAddress()` then gives it a free frame.
- **What is the CPI?** Cycles / instructions: 93 / 71 = 1.31 for sum_array, 329 / 15 = 21.9 for pages, whose 3 page faults dominate.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
