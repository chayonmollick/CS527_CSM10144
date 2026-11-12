# Q065 — Frame map after each task finishes

## Question
After each task finishes, print a map of physical memory with one character per frame:
`R` for the reserved page-table frame, `.` for a free frame, and for a used frame the pid of
the task that owns it (a digit; `#` if the pid is 10 or more). Run `sum_array`, `pages`
and a 3-instruction program together and explain each map, in particular the hole that
appears in it.

## Answer
The OS keeps only `freePages[]` (used or free), not *who* owns a frame. But every used
frame is an entry in exactly one page table in frame 0, so the owner is found by reading
the page tables of the running tasks:

```c
static void print_frame_map(int finished_pid) {
    std::string map;
    for (int f = 0; f < NUM_PHYSICAL_PAGES; f++) map += f == 0 ? 'R' : freePages[f] ? '?' : '.';
    for (int p = 0; p < NP; p++)
        for (int i = 0; proc_owner[p] && i < NUM_LOGICAL_PAGES; i++)
            if (int f = memory[PTBR[p] + i]) map[f] = proc_owner[p] < 10 ? (char)('0' + proc_owner[p]) : '#';
    std::printf("[os] frames after pid %d finished: %s\n", finished_pid, map.c_str());
}
```
`finish_task()` calls it after `finalize()` (which freed the frames and cleared the table)
and after `proc_owner[p] = 0`, so the finished task is no longer in the map. A used frame
that is in no table would show `?`, which would be a leak; it never appears.

With pid 1 = `sum_array`, pid 2 = `pages`, pid 3 = `tiny`:
```
[os] frames after pid 3 finished: R1122..222......
[os] frames after pid 2 finished: R11.............
[os] frames after pid 1 finished: R...............
```
- **Loading:** the loader gives the lowest free frames: pid 1 gets 1-2, pid 2 gets 3-4,
  pid 3 gets 5-6.
- **Round 1:** `pages` (pid 2) runs before `tiny` and demand-maps data pages 1, 3, 7 to
  frames 7, 8, 9. Then `tiny` ends and frees 5-6. That leaves a hole in the middle of
  pid 2's frames: pages do not need contiguous frames, so this is no problem.
- **Round 2:** `pages` ends. **Round 8:** `sum_array` ends.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `print_frame_map` (new) | `R`/`.`/`?` from `freePages`, owners from every running task's page table (`memory[PTBR[p] + i]`) |
| `os.cpp` | `finish_task` | call `print_frame_map(pid)` after the task's frames are freed |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` checks the three maps above, and that a map has 16 characters. Then it runs
9 × `tiny` + `sum_array`: four tasks run per round, so pids 9 and 10 load in round 3 on
frames 1-2 and 3-4. The map after pid 1 is `R..223344.......` and the map after pid 9 is
`R..##...........` (pid 10 is shown as `#`). `sum_array` still gives 45.

## Viva
- **Why not keep an owner array per frame?** The same fact would then be stored twice and could disagree. The page tables already say which frame belongs to whom. `extensions/e15`'s `mem` dump uses the same idea, with one line per frame instead of one character.
- **Is the hole a problem (external fragmentation)?** No. Paging lets any page go into any frame, so free frames never need to be adjacent. That is the main reason for paging.
- **Why is the finished task not in its own map?** `finalize()` has already freed its frames and zeroed its table. The map shows what the remaining tasks hold and what the next task can get.
- **Why `#` for pid 10?** With one character per frame the map stays 16 characters wide. A two-digit pid would shift every following column.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `tiny.txt`, `tiny_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
