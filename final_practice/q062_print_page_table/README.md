# Q062 — Print a task's page table when it finishes

## Question
When a task finishes, print its page table: for each of its 10 logical pages
(instruction pages I0, I1 and data pages D0..D7) the frame it is mapped to, or `-` if it
is not mapped. The table must be read from physical frame 0 through the task's PTBR,
not from a copy kept elsewhere. Run `sum_array` and `pages` together and explain every
frame number in the output.

## Answer
In `finalize()`, after the output files are written and **before** the loop that frees
the frames and zeroes the entries, read the 10 entries at `memory[PTBR[p] + i]`:

```c
std::printf("[os] pid %d page table (PTBR %d):", t.pid, PTBR[p]);
for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {
    bool code = i < NUM_INSTR_PAGES;
    int frame = memory[PTBR[p] + i];
    std::printf(" %c%d=%s", code ? 'I' : 'D', code ? i : i - NUM_INSTR_PAGES, frame ? std::to_string(frame).c_str() : "-");
}
```
Index `i < 2` is an instruction page, otherwise data page `i - 2`; an entry of 0 means
not mapped, because frame 0 holds the tables and is never given to a task.

Expected output:
```
[os] pid 2 page table (PTBR 10): I0=3 I1=- D0=4 D1=5 D2=- D3=6 D4=- D5=- D6=- D7=7
[os] pid 1 page table (PTBR 0): I0=1 I1=- D0=2 D1=- D2=- D3=- D4=- D5=- D6=- D7=-
```
- The loader takes the lowest free frames: pid 1 (processor 0, PTBR 0) gets 1 and 2,
  and pid 2 (processor 1, PTBR 1 × 10 = 10) gets 3 and 4.
- In round 1, `pages.txt` stores to 1020, 2040 and 3825, which are data pages 1, 3 and 7.
  They are demand-mapped to the next free frames: 5, 6 and 7.
- `pages` finishes first (15 instructions, round 2), so its table is printed first.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `finalize` | before the frames are released: print `I0..I1, D0..D7 = frame` (or `-`) read from `memory[PTBR[p] + i]` |

The exact code is in [`solution.patch`](solution.patch) (one hunk, 8 lines).

## Test
`./test.sh` runs `sum_array` and `pages` together and checks both lines character by
character (the frame numbers above), that the table comes before the task's `finished`
line, that `pages` still gives 21, and that `sum_array` alone still gives 45.

## Viva
- **Why print it before the release loop?** That loop frees each frame and sets the entry to 0, and then `PTBR[p] = -1`. Afterwards the table is gone.
- **Where exactly is pid 2's table?** Bytes 10..19 of physical memory (frame 0). D1 is the entry at byte 10 + 3 = 13, and it holds 5, so logical data address 1020 is physical 5 × 512 + 508 = 3068.
- **Why does pid 1 have I1 = `-`?** Its code is 13 instructions = 52 bytes, so it fits in instruction page 0. The loader maps only the pages the program and the data file use.
- **Why read through PTBR instead of `p * 10`?** PTBR is the hardware's view: the MMU knows only the register. If the OS ever moved a table (Q074), code that recomputes `p * 10` would read the wrong bytes.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
