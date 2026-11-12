# Q070 — Report internal fragmentation per task

## Question
Paging has no external fragmentation, but it wastes the unused end of the last page.
When a task is loaded, report its internal fragmentation: the number of unused bytes in
its last instruction page and in its last data page (the pages the loader fills from the
program and the data file), and their sum. Give the numbers for `sum_array`, `pages`,
`array_add` and `fir`, and for data files of exactly 512 and 516 bytes.

## Answer
The loader knows both sizes: `t.prog.code.size()` (4 bytes per instruction, including the
end instruction) and `t.data.size()`. A segment of `size` bytes takes
`pages_for(size) = ceil(size / 512)` pages, so its last page has
```
unused = pages_for(size) * PAGESIZE - size
```
bytes free. `loader()` prints it after the "loaded" line:
```c
int code_waste = pages_for(t.prog.code.size()) * PAGESIZE - (int)t.prog.code.size();
int data_waste = pages_for(t.data.size()) * PAGESIZE - (int)t.data.size();
```

| Task | Code | Unused | Data file | Unused | Total |
|---|---|---|---|---|---|
| sum_array | 13 instr. = 52 B | 460 | 40 B | 472 | 932 |
| pages | 15 instr. = 60 B | 452 | 4 B | 508 | 960 |
| array_add | 18 instr. = 72 B | 440 | 132 B | 380 | 820 |
| fir | 53 instr. = 212 B | 300 | 132 B | 380 | 680 |
| counter + 512-byte data | 7 instr. = 28 B | 484 | 512 B = 1 page | 0 | 484 |
| counter + 516-byte data | 28 B | 484 | 516 B = 2 pages | 1024 − 516 = 508 | 992 |

The four example tasks fill 8 frames (4096 bytes) but use only 704 of them; 3392 bytes
(83%) are internal fragmentation, because the programs are much smaller than a page.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `loader` | after the "loaded" message: compute `pages * PAGESIZE - size` for the code and the data file, print both and the sum |

The exact code is in [`solution.patch`](solution.patch) (4 lines).

## Test
`./test.sh` checks the six lines of the table, character by character. The 512- and
516-byte data files (128 and 129 zero words) are generated in the test's run folder. It
also checks that `sum_array` alone gives 45.

## Viva
- **Why `pages * PAGESIZE - size` and not `PAGESIZE - size % PAGESIZE`?** The second formula gives 512 instead of 0 when the size is an exact multiple of the page size (the 512-byte file).
- **How does the page size change it?** On average half a page is wasted per segment, so smaller pages waste less. With 256-byte pages `sum_array` wastes 204 + 216 = 420 bytes instead of 932, but it needs page tables twice as large (see Q061).
- **What about demand-mapped pages?** They waste space too: `pages.txt` uses 4 bytes in each of its 3 demand-mapped pages. The question counts only the pages the loader fills, whose size is known at load time.
- **Why is there no external fragmentation?** Any free frame can hold any page, so free memory is never "free but unusable" because it is in small separate pieces, as it is with contiguous allocation.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `array_add.txt`, `array_add_data.byte`, `counter.txt`, `fir.txt`, `fir_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
