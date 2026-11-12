# Q063 — Report which data pages page-faulted

## Question
The simulator only prints *how many* page faults a task had. When a task finishes, also
print *which* data pages page-faulted, in the order the faults happened (or `none`).
Write a program that touches data pages 5, 2 and 6 in that order, and then stores one word
that crosses a page boundary. Run it together with `pages.txt` and `sum_array`, and
explain each list.

## Answer
A page fault happens in exactly one place: the "frame == 0" branch of
`getPhysicalAddress()` for a data access. Record the page there, per processor (the
same pattern as `page_faults_on[p]`):

```c
static std::vector<int> faulted_pages[NP];             /* next to page_faults_on */
...  page_faults_on[p]++;
     faulted_pages[p].push_back(index - NUM_INSTR_PAGES); /* table index -> data page */
```
`initialize()` clears the list when a task is loaded, and `finalize()` prints it after
the `finished` line.

Expected lines:
```
[os] pid 2 page faults on data pages: 1 3 7        pages.txt: 1020, 2040, 3825
[os] pid 3 page faults on data pages: 5 2 6 3 4    scatter.txt
[os] pid 1 page faults on data pages: none         sum_array: only its data file
```
- **scatter.txt:** it stores to 2560 (page 5), 1024 (page 2) and 3072 (page 6). Then it
  stores the word at 2046: bytes 2046-2047 are in page 3 and bytes 2048-2049 in page 4.
  The MMU translates byte by byte (lowest address first), so this one store faults
  twice: 3, then 4.
- **Reading back:** the later loads touch the same pages, so there are no new faults.
  The sum is 1 + 2 + 3 + 4 = 10.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | (globals) | `std::vector<int> faulted_pages[NP]` |
| `os.cpp` | `getPhysicalAddress` | on a data page fault: `faulted_pages[p].push_back(index - NUM_INSTR_PAGES)` |
| `os.cpp` | `initialize` | `faulted_pages[p].clear()` |
| `os.cpp` | `finalize` | print the list, or `none` |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs `sum_array`, `pages` and `scatter` together. It checks the three lines
above, that pid 3 still reports `5 page fault(s)`, that scatter computes 10 and that the
word at 2046 (across the page boundary) reads back as 4, and that `sum_array` alone gives 45.

## Viva
- **Why record in `getPhysicalAddress` and not in the processor?** Only the MMU knows that a page was unmapped. The processor just sees a physical address come back.
- **Could `finalize()` find the list without recording?** Partly: every mapped data page beyond the data file's pages must have faulted. But that loses the order, so recording at fault time is simpler and exact.
- **Why can one instruction fault twice?** A 4-byte word at 2046 spans two pages, and each byte is translated separately. A vector store (8 words) can fault on even more pages.
- **Is the list per processor or per task?** It is per processor, but it is cleared when a task is loaded and printed when that task ends. Since a task keeps its processor, that makes it per task.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `pages.txt`, `pages_data.byte`, `scatter.txt`, `scatter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
