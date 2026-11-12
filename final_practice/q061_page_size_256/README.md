# Q061 — Page size 256 (config only)

## Question
The simulator uses 512-byte pages. Change the page size to 256 bytes without editing any
`.cpp` file. How many instruction pages, data pages, page-table entries and frames are
there now, and how much of frame 0 do the page tables use? Show that the example programs
still give the same results and how many frames each task uses now. Why does `fir` now
take a page fault that it did not take before?

## Answer
Every size is derived from `PAGESIZE` in `config.h`, so the answer is one `#define`; the
four derived macros only get new comments. Then `make clean && make`.

| | 512 (before) | 256 (now) |
|---|---|---|
| instruction pages = 1024 / PAGESIZE | 2 | 4 |
| data pages = 4096 / PAGESIZE | 8 | 16 |
| page-table entries per task (`NUM_LOGICAL_PAGES`) | 10 | 20 |
| frames = 8192 / PAGESIZE (frame 0 reserved) | 16 (15 usable) | 32 (31 usable) |
| page tables in frame 0 = NP × entries | 40 of 512 bytes | 80 of 256 bytes |
| `PTBR[p] = p * NUM_LOGICAL_PAGES` | 0, 10, 20, 30 | 0, 20, 40, 60 |

Translation keeps its form: `index = address / 256` (+ 4 for data), `physical = frame * 256
+ address % 256`. Both `static_assert`s in `os.cpp` still hold (80 <= 256, 32 <= 256).

Frames per task = instruction pages + data-file pages + data pages touched later:

| Task | Code | Data file | Touched later | Frames at 256 | At 512 |
|---|---|---|---|---|---|
| sum_array | 52 B → 1 | 40 B → 1 | – | 2 (512 B) | 2 (1024 B) |
| pages | 60 B → 1 | 4 B → 1 | 1020, 2040, 3825 → pages 3, 7, 14 | 5 (1280 B) | 5 (2560 B) |
| fir | 212 B → 1 | 132 B → 1 | output at 256 → page 1 | 3 (768 B) | 2 (1024 B) |
| long70 (73 instr.) | 292 B → 2 | 4 B → 1 | – | 3 (768 B) | 2 (1024 B) |

`fir` writes its output from address 0x100 = 256. With 512-byte pages that is still data
page 0 (loaded from the file); with 256-byte pages it is data page 1, which is not in the
file, so it is demand-mapped: one page fault. The results do not change, because paging
only changes *where* the bytes are, not their values.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | — | `PAGESIZE 512` → `256`; comments of `NUM_INSTR_PAGES`, `NUM_DATA_PAGES`, `NUM_LOGICAL_PAGES`, `NUM_PHYSICAL_PAGES` updated |

The exact lines are in [`solution.patch`](solution.patch). Apply it to a copy of `lab6`
with `patch -p1 < solution.patch`.

## Test
`./test.sh` runs `sum_array`, `pages`, `fir` and a generated 73-instruction program
(`long70`, 70 increments) at once. The loader lines must show 29, 27, 25 and 22 frames
left of 31 (long70 needs 2 instruction pages now), `pages` 3 page faults, `fir` 1 page
fault, and the results 21, 204 (`fir` out[0] = 1² + … + 8²), 70 and 45.

## Viva
- **Why not 128-byte pages?** 8 + 32 = 40 entries × 4 processors = 160 bytes, more than one 128-byte frame: the page tables no longer fit in frame 0 and the `static_assert` stops the build. 256 is the smallest power of two that works with NP = 4.
- **What do smaller pages give and cost?** Less internal fragmentation (sum_array occupies 512 bytes instead of 1024), but twice as many page-table entries, more page faults (`fir`), and more translations of different pages.
- **Why does nothing else change?** The code only uses the macros (`PAGESIZE`, `NUM_INSTR_PAGES`, `NUM_LOGICAL_PAGES`...), never 512 or 2; only some comments in `os.cpp` still say "data pages 2..9".
- **Why `make clean && make`?** Every `.o` has the old sizes compiled in, so all of them must be rebuilt. A stale object built with 512 would disagree with the others about the memory layout. This Makefile makes every object depend on `config.h`, so plain `make` also works, but in a copied folder the timestamps can mislead `make`.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `fir.txt`, `fir_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
