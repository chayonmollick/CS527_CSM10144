# Q064 — Per-task frame limit (`MAX_FRAMES_PER_TASK`)

## Question
One task can take every free frame by touching many data pages. Add `MAX_FRAMES_PER_TASK`
(4) to `config.h`: a task may hold at most that many frames, counting its code pages, its
data-file pages and the data pages mapped on demand. When a page fault would give a task
one frame more, stop that task with "out of memory"; the other tasks keep running. Refuse
a task that needs more than the limit just to load. Show it with `pages.txt`.

## Answer
- **How many frames does a task hold?** Exactly the number of non-zero entries in its
  page table, because frames are never shared. So no new counter is needed:
  `frames_of(p)` counts the mapped entries at `memory[PTBR[p] + i]`.
- **Page fault:** in `getPhysicalAddress()`, in the demand-mapping branch, check before
  `getFreePage()`:
  ```c
  if (frames_of(p) >= MAX_FRAMES_PER_TASK) {
      processor_error("out of memory: data page %d would be frame %d of this task (MAX_FRAMES_PER_TASK %d)", ...);
      return -1;
  }
  ```
  `processor_error` stops only the current task. `finalize()` still writes its output
  and frees its frames.
- **Loader:** `create_task()` refuses a task whose code pages + data-file pages already
  exceed the limit. Otherwise it would be over its quota before running one instruction.

`pages.txt` starts with 2 frames (code page, data page 0). Its stores to 1020 and 2040
demand-map data pages 1 and 3, which become its 3rd and 4th frames. The store to 3825
(data page 7, instruction 7) would need a 5th frame, so the task stops after 8
instructions with 2 page faults. Its sum at 0 is never written (stays 0), but 7 is at 1020 and 2040. `sum_array`,
running at the same time, still gives 45.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | — | `#define MAX_FRAMES_PER_TASK 4` |
| `os.cpp` | `frames_of` (new) | count the mapped entries of `p`'s page table |
| `os.cpp` | `getPhysicalAddress` | on a data page fault: `frames_of(p) >= MAX_FRAMES_PER_TASK` → "out of memory", return -1 |
| `os.cpp` | `create_task` | refuse a task whose code + data-file pages exceed the limit |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs `pages` + `sum_array` and checks:
- the error text at instruction 7 and the `STOPPED WITH AN ERROR ... after 8 instructions, 2 page fault(s)` line;
- the partial output: 7 at 2040 and 0 at 0;
- that `sum_array` = 45 in the same run;
- that `sum_array` with a 2048-byte data file (4 data pages + 1 code page = 5 frames) is refused at load.

## Viva
- **How is this different from "no free physical frame"?** That error means memory as a whole is full. This one is a quota that stops one greedy task before it hurts the others, like `ulimit -v` or a cgroup `memory.max`.
- **Why count page-table entries instead of keeping a counter?** The table is already the exact record of what the task holds. A separate counter would have to be kept in step in three places: loader, fault and finalize.
- **What would a real OS do instead of stopping the task?** Evict one of the task's *own* pages to disk and reuse that frame (local replacement, `extensions/e13_page_replacement`). The limit is then the task's resident-set size.
- **Does a fault on an instruction page count?** Here instruction pages are all loaded at start and never fault. They do count toward the limit, because they are entries in the same table.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
