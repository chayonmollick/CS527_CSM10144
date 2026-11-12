# Q074 — Page tables in the last frame instead of frame 0

## Question
Lab 6 keeps the page tables in physical frame 0. Move them to the **last** frame (15), so
that frame 0 becomes an ordinary frame that tasks can use. What are the PTBR values now?
A page-table entry of 0 can no longer mean "not mapped": what marker do you use instead,
and which places in the code depend on it? Show that the programs still work and that
frame 0 really is used.

## Answer
- **Where the tables are:** `PT_FRAME = NUM_PHYSICAL_PAGES - 1 = 15`, so
  `PTBR[p] = PT_FRAME * 512 + p * 10`: 7680, 7690, 7700, 7710.
  `os_init()` reserves `freePages[PT_FRAME]` instead of `freePages[0]`.
- **The marker:** `NOT_MAPPED = PT_FRAME` (15). The frame that holds the tables is never
  given to a task, so its number can never be a real mapping. This is exactly why 0
  worked before. (Any value >= 16, such as `0xFF`, would also work.) `initialize()` fills
  a new table with `memset(..., NOT_MAPPED, 10)`, and `finalize()` writes `NOT_MAPPED`
  back.
- **Every place that treated 0 as "not mapped" or "reserved":**

| Place | Was | Now | If you forget it |
|---|---|---|---|
| `getPhysicalAddress` | `frame == 0` | `frame == NOT_MAPPED` | frame 0 looks unmapped; an unmapped entry (15) points into the page tables |
| `finalize`, write-back | `if (frame)` | `!= NOT_MAPPED` | a page in frame 0 is missing from the `.out` file |
| `finalize`, release | `if (frame)` | `!= NOT_MAPPED` | frame 0 is never freed (a leak) |
| `getFreePage`, `free_page_count` | `f = 1` | `f = 0` | frame 0 is never used |

To make the layout visible, `initialize()` prints the PTBR and the frame of
instruction page 0.

Run `tiny`, `pages`, `sum_array`, `counter` and a second `tiny` (pids 1-5; pid 5 waits):

| When | What happens | Frames |
|---|---|---|
| load | pid 1 `tiny`: PTBR 7680, code in **frame 0** | 0, 1 (13 left) |
| load | pids 2, 3, 4: PTBR 7690, 7700, 7710 | 2-3, 4-5, 6-7 (7 left) |
| round 1 | `tiny` ends and frees 0, 1; `pages` maps data pages 1, 3, 7 | **0**, 1, 8 |
| round 2 | pid 5 loads | 9, 10 (4 left) |

Data page 1 of `pages` (address 1020) is in frame 0, and `pages` still gives 21, with 7 at
1020 in its `.out` file. There are still 15 usable frames (13 left after the first task).

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | (top) | `#define PT_FRAME (NUM_PHYSICAL_PAGES - 1)`, `#define NOT_MAPPED PT_FRAME` |
| `os.cpp` | `free_page_count`, `getFreePage` | loops start at frame 0 |
| `os.cpp` | `getPhysicalAddress` | `frame == NOT_MAPPED` |
| `os.cpp` | `initialize` | `PTBR[p] = PT_FRAME * PAGESIZE + p * NUM_LOGICAL_PAGES`; fill with `NOT_MAPPED`; print the layout |
| `os.cpp` | `finalize` | `!= NOT_MAPPED` in the write-back and release loops; clear entries to `NOT_MAPPED` |
| `os.cpp` | `os_init` | reserve `freePages[PT_FRAME]` |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs the 5 tasks and checks:
- the 5 layout lines (PTBR 7680/7690/7700/7710/7680, code frames 0/2/4/6/9);
- the frames left after each load: 13 11 9 7, then 4;
- the results: `pages` 21 with 7 at 1020 (written back from frame 0), `counter` 10, `tiny` 5.

It also checks that `sum_array` alone gives 45. The values were also checked against two
wrong versions. Keeping `if (frame)` in the write-back gives 0 at 1020. Not freeing
frame 0 puts pid 5 in frame 10 with 3 frames left.

## Viva
- **Why can 0 no longer mean "not mapped"?** Frame 0 is a real frame now: the first task's code is in it. An entry of 0 must mean "frame 0".
- **Why is 15 a safe marker?** `freePages[15]` is set at start-up and never cleared, so `getFreePage()` never returns 15, and no entry can hold 15 as a real mapping.
- **What is better than a magic frame number?** A VALID bit in the entry (`extensions/e14_pte_flags_protection`): any frame number can then be stored, and "not mapped" is just VALID = 0.
- **Did the page-table size change?** No. Each table is still 10 bytes at `PTBR[p]`; only the base moved, so `getPhysicalAddress` (`memory[PTBR[p] + index]`) needed no change except the marker.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `tiny.txt`, `tiny_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
