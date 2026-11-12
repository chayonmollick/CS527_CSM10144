# Q073 — Data-file pages plus one extra page

## Question
A task may use the data pages of its data file and **one** extra data page (for example
for its results). Touching any further data page is a segmentation fault: the task is
stopped, the other tasks keep running. Show that `pages.txt` now fails, that a program
using a single extra page works, and what changes when `pages.txt` gets a 2-page data file.

## Answer
The loader maps exactly the data-file pages. Every later data page fault therefore maps
a page **beyond** the data file, and the per-task fault counter `page_faults_on[p]` is the
number of extra pages already in use. One check before the demand mapping in
`getPhysicalAddress()` enforces the rule:
```c
if (page_faults_on[p] >= EXTRA_DATA_PAGES) {     /* config.h: EXTRA_DATA_PAGES 1 */
    processor_error("segmentation fault: data page %d is not in the data file and the %d extra page(s) are used", ...);
    return -1;
}
```

| Run | Data-file pages | Extra page | Fault |
|---|---|---|---|
| `pages.txt` + 4-byte file | 0 | 1 (store to 1020) | page 3 (store to 2040, instruction 5): stops after 6 instructions |
| `extra.txt` + 4-byte file | 0 | 2 (1024 and 1028) | none: finishes, 41 and 42 written |
| `pages.txt` + 516-byte file | 0, 1 | 3 (store to 2040) | page 7 (store to 3825, instruction 7): stops after 8 instructions |

With the 2-page data file, page 1 belongs to the file and costs nothing, so the extra
page is page 3 and the program gets one store further. In all cases the stores before
the fault are in the output file (7 at 1020, and at 2040 in the third run).

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | — | `#define EXTRA_DATA_PAGES 1` |
| `os.cpp` | `getPhysicalAddress` | before demand-mapping a data page: `page_faults_on[p] >= EXTRA_DATA_PAGES` → "segmentation fault", return -1 |

The exact code is in [`solution.patch`](solution.patch): one `#define` and a 5-line check.

## Test
`./test.sh` runs `pages`, `extra` and `sum_array` together. It checks the fault message
and the stop after 6 instructions with 1 page fault, the extra page's 7, `extra`'s 41/42
with 1 page fault, and 45. It then runs `pages.txt` on a generated 516-byte data file: the
fault moves to page 7 at instruction 7 (after 8 instructions), and 7 is stored at 2040.
It also checks that `sum_array` alone gives 45.

## Viva
- **Why is the fault counter enough?** Faults happen only for pages that are not in the data file (the loader mapped all of those), and each fault maps exactly one new page. So faults = extra pages in use.
- **How is this different from `STRICT_MEMORY` in `extensions/e14`?** e14 allows **no** page outside the data file and uses a VALID bit. This allows a fixed budget of extra pages: the same idea as a stack or heap limit (`ulimit -s`).
- **Is this a page fault or a segmentation fault?** Both happen: the hardware raises a page fault (unmapped page); the OS then decides the address is not allowed and turns it into a segmentation fault (SIGSEGV) instead of mapping a page.
- **What does the stopped task's output contain?** `finalize()` still runs: everything stored before the fault is written to the `.out` file, and the frames are freed.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `extra.txt`, `extra_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
