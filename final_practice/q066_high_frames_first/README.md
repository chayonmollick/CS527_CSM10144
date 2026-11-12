# Q066 — Allocate frames from the highest number down

## Question
`getFreePage()` returns the lowest-numbered free frame. Change it to return the
highest-numbered free frame (frame 0 must stay reserved). Show that the results of the
programs do not change but the page-to-frame mapping does: which frames do `sum_array`
and `pages` get when they run together, compared with before? Why can a program not
notice the difference?

## Answer
One loop in `getFreePage()` runs the other way:
```c
for (int f = NUM_PHYSICAL_PAGES - 1; f >= 1; f--)
    if (!freePages[f]) { freePages[f] = 1; return f; }
```
The loader (`copy_in`) and the demand-mapping in `getPhysicalAddress` both call
`getFreePage()`, so both follow the new order. Nothing else changes: `free_page_count()`
and the loader's "enough frames" check do not depend on the order.

The mapping is invisible in the output files, so to *show* it, `finalize()` lists the
frames it releases, in page-table order (I0, I1, D0..D7):

| Task | Pages | lab6 (lowest first) | now (highest first) |
|---|---|---|---|
| pid 1 sum_array | I0, D0 | 1 2 | 15 14 |
| pid 2 pages | I0, D0, then D1, D3, D7 on demand (round 1) | 3 4 5 6 7 | 13 12 11 10 9 |

The program cannot notice, because it only ever uses logical addresses. Every access is
translated through its own page table, which now holds other frame numbers. Only the
physical address `frame * 512 + offset` changes, never the value stored there.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `getFreePage` | loop `f = NUM_PHYSICAL_PAGES - 1` down to 1 |
| `os.cpp` | `finalize` | collect the frames in the release loop; print `pid N released frames: ...` |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs `sum_array` and `pages` and checks the frame lists `15 14` and
`13 12 11 10 9`, the unchanged free-frame count (11 after both loads), and 21 for `pages`.
It then builds the unmodified `lab6`, runs the same two tasks, and checks with `cmp` that
both `.out` files and both `.freq` files are byte-for-byte identical. It also checks that
`sum_array` alone still gives 45.

## Viva
- **Why stop at `f >= 1`?** Frame 0 holds the page tables. `freePages[0] = 1` would already skip it, but stopping at 1 makes the reservation visible and does not depend on that flag.
- **Does the order matter for correctness?** No, as long as the free list is right. That is the point of paging: any free frame is as good as any other.
- **When does it matter in real systems?** For performance or hardware limits, not correctness. Examples are cache colouring, keeping low memory free for old DMA devices (Linux `ZONE_DMA`), and making large contiguous blocks easier to find.
- **Is any frame freed in a different order now?** `finalize()` still walks the page table from index 0, so pid 2 frees 13, 12, 11, 10, 9. Freeing order does not matter either, because the next `getFreePage()` scans the whole array.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
