# e15 — Memory-map dump (`mem` command and memory_map.txt)

## Question (as it could appear in the test)
Add a shell command `mem` that prints the memory map of the simulated machine. For every
physical frame, show the owner pid, the processor, the logical page number, and whether
it is an instruction page, a data page or the reserved page-table frame. Then print every
processor's page table, read from frame 0 through PTBR, and the number of free frames.
Also write `memory_map.txt` at the end of every scheduler round in which the memory map
changed (and at exit). Run 3 tasks and show that the map is consistent: every used frame
appears in exactly one page table.

## Solution idea
- **One function, two outputs:** `dump_memory_map(FILE *f, title)` in `os.cpp`. The `mem`
  command calls it with `stdout`. `write_memory_map()` appends it to `memory_map.txt`, which
  `os_init()` empties at start-up.
- **Frame section:** whether a frame is used comes from the OS free list `freePages[]`.
  *Who* uses it is found by searching every page table in frame 0 (`memory[PTBR[p] + i]`
  for all busy processors). So the dump needs no new bookkeeping. Each frame gets one line:
  - frame 0: `reserved: page tables`;
  - a used frame found in exactly one entry: `pid P  proc Q  instr|data page I`
    (index < 2 = instruction page, else data page index - 2);
  - an unused frame found in no entry: `free`;
  - anything else is printed as `INCONSISTENT` and counted. That covers a leaked frame
    (allocated, in no table), a double mapping (in two tables) and a use-after-free
    (free but still mapped).
- **Page-table section:** for each processor, `PTBR`, pid and program name, then
  `I0 I1 D0..D7 = frame` or `-`. An idle processor shows `PTBR -1`. Last come
  `free frames: N of 15` and `check: OK` (or `check: N PROBLEM(S)`).
- **When to write a snapshot:** `map_changed` is set where frames change hands:
  `getFreePage()` (the loader and every demand mapping) and `finalize()` (freeing). At the
  end of each scheduler round, after the shell (a `mem`/new task in that round is
  included), the scheduler writes `after round N` if the flag is set and clears it. After
  the loop it writes `at exit`, where every frame must be free again (no leak).
- **Example** (`grow.txt` + pages.txt + sum_array.txt): grow's loop is 9 instructions, so
  it touches data page p in round p.
  - **Free frames:** 15 - 3 tasks x 2 = 9. After round 1: 5 (grow page 1, pages.txt
    pages 1, 3, 7). Round 2: 9 (grow page 2; pages.txt finished and returned 5 frames).
    Rounds 3-6: 8, 7, 6, 5. Round 7: 13 (grow's page 7, then grow returned 9). Round 8:
    15 (sum_array ended). At exit: 15.
  - **Snapshots:** rounds 1-8 + exit = 9.
  - **What the map shows:** a task's pages are scattered (grow: D0=2 D1=7 D2=11 D3=3),
    and freed frames are reused at once (frame 3 belonged to pages.txt in round 1).
- **Independent check in test.sh:** an awk script re-reads every snapshot and checks it:
  - each used frame appears exactly once in the page tables, with the same pid,
    processor and page;
  - free frames appear in no table;
  - the free count matches.

  To prove the checker can fail, it is also run on a snapshot where one entry was
  changed to point at a free frame. It finds 3 problems.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | | `#define MEMORY_MAP_FILE "memory_map.txt"` |
| `os.cpp` | (globals) | `bool map_changed` |
| `os.cpp` | `getFreePage()` | sets `map_changed` when it allocates a frame |
| `os.cpp` | `finalize()` | sets `map_changed` after freeing the task's frames |
| `os.cpp` | `dump_memory_map()` | new: frame list (owner from a search of all page tables), page tables via PTBR, free count, consistency check |
| `os.cpp` | `write_memory_map()` | new: appends a snapshot to memory_map.txt, clears `map_changed` |
| `os.cpp` | `run_command()` | new shell command `mem`; usage message mentions it |
| `os.cpp` | `os_init()` | empties memory_map.txt |
| `os.cpp` | `scheduler()` | round counter; snapshot after every round that changed the map, and at exit; welcome message mentions `mem` |
| `Makefile` | `test`, `clean` | runs `./test.sh` + the 33 base checks; `clean` also removes memory_map.txt |

## Run
```bash
make
./sim -b grow.txt grow_data.byte ../../examples/pages.txt ../../examples/pages_data.byte \
         ../../examples/sum_array.txt ../../examples/sum_array_data.byte
less memory_map.txt                          # 9 snapshots
./sim                                        # interactive: type a task, then mem, then exit
./test.sh                                    # 13 checks
SIM=$PWD/sim ../../tests/run_tests.sh        # 33 passed
```

## Expected output
`memory_map.txt`, first snapshot (pid 1 = grow, pid 2 = pages, pid 3 = sum_array):
```
=== memory map: after round 1 ===
frame  0  reserved: page tables (processor p at bytes 10*p .. 10*p+9)
frame  1  pid 1  proc 0  instr page 0
frame  2  pid 1  proc 0  data page 0
frame  3  pid 2  proc 1  instr page 0
frame  4  pid 2  proc 1  data page 0
frame  5  pid 3  proc 2  instr page 0
frame  6  pid 3  proc 2  data page 0
frame  7  pid 1  proc 0  data page 1
frame  8  pid 2  proc 1  data page 1
frame  9  pid 2  proc 1  data page 3
frame 10  pid 2  proc 1  data page 7
frame 11  free
frame 12  free
frame 13  free
frame 14  free
frame 15  free
page tables in frame 0 (entry = frame, - = not mapped):
proc 0  PTBR 0   pid 1 (grow): I0=1 I1=- D0=2 D1=7 D2=- D3=- D4=- D5=- D6=- D7=-
proc 1  PTBR 10  pid 2 (pages): I0=3 I1=- D0=4 D1=8 D2=- D3=9 D4=- D5=- D6=- D7=10
proc 2  PTBR 20  pid 3 (sum_array): I0=5 I1=- D0=6 D1=- D2=- D3=- D4=- D5=- D6=- D7=-
proc 3  idle (PTBR -1)
free frames: 5 of 15
check: OK (every used frame is in exactly one page table, no free frame is mapped)
```
Round 3 (pages.txt has finished; grow got frame 11 in round 2 and frame 3, pages.txt's old frame, in round 3):
```
proc 0  PTBR 0   pid 1 (grow): I0=1 I1=- D0=2 D1=7 D2=11 D3=3 D4=- D5=- D6=- D7=-
proc 1  idle (PTBR -1)
proc 2  PTBR 20  pid 3 (sum_array): I0=5 I1=- D0=6 D1=- D2=- D3=- D4=- D5=- D6=- D7=-
free frames: 8 of 15
```
The shell command:
```
$ printf 'sum_array.txt sum_array_data.byte\nmem\nexit\n' | ./sim
[shell] type '<program> <data.byte>' to start a task, 'mem' for the memory map, 'exit' to stop accepting tasks
$ [os] pid 1 created: sum_array.txt sum_array_data.byte (13 instructions)
[os] pid 1 (sum_array) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
$ === memory map: shell command mem ===
frame  0  reserved: page tables (processor p at bytes 10*p .. 10*p+9)
frame  1  pid 1  proc 0  instr page 0
frame  2  pid 1  proc 0  data page 0
frame  3  free
...
frame 15  free
page tables in frame 0 (entry = frame, - = not mapped):
proc 0  PTBR 0   pid 1 (sum_array): I0=1 I1=- D0=2 D1=- D2=- D3=- D4=- D5=- D6=- D7=-
proc 1  idle (PTBR -1)
proc 2  idle (PTBR -1)
proc 3  idle (PTBR -1)
free frames: 13 of 15
check: OK (every used frame is in exactly one page table, no free frame is mapped)
```
`./test.sh`:
```
e15: 3 tasks (grow.txt grows one data page per round, pages.txt, sum_array.txt)
  ok    results: grow 1+..+7 = 28 and page 7 holds 7, pages 21, sum_array 45
  ok    memory_map.txt: a snapshot after rounds 1..8 and one at exit
  ok    awk re-check: 9 snapshots, 0 problems
  ok    the simulator's own check says OK in all 9 snapshots
  ok    free frames per snapshot: 5,9,8,7,6,5,13,15,15
  ok    round 1: pid 2 (pages) has data pages 0, 1, 3, 7 mapped
  ok    round 3: grow's pages are scattered and reuse pages.txt's old frame 3
  ok    at exit: all 15 frames free, all processors idle
e15: the checker itself notices a corrupted map
  ok    corrupted snapshot (grow's D1 -> free frame 11): 3 problems
e15: shell command mem
  ok    mem prints a map with pid 1's two frames and 13 free
  ok    mem output passes the awk check
  ok    the task still runs to the right result
  ok    'mem a b' is not the mem command: one usage message, no map

13 passed, 0 failed
```

## Viva questions
- **How do you find the owner of a frame when page tables only map page → frame?** Search
  all page tables for entries equal to the frame (the inverse mapping). With 4 tables x 10
  entries that is cheap. Real OSes keep a frame table / inverted page table instead
  (Linux: `struct page`), which e13 adds as `frame_info[]`.
- **What would an INCONSISTENT line point to?**
  - "allocated, in 0 entries": a leaked frame (allocated but never mapped, or the
    page-table entry was cleared without freeing it).
  - "free, in 1 entry": use-after-free (a task can still reach a frame that may be given
    to another task).
  - "in 2 entries": two pages share one frame, so they corrupt each other.
- **Why is the snapshot written at the end of the round and not inside `getFreePage()`?**
  One round can allocate several frames (pages.txt maps 3 pages in round 1). Mid-update
  the map can also be briefly half-done: the frame is taken but not yet in the page
  table. At a round boundary the map is stable, and the file stays short.
- **Why does the "at exit" snapshot matter?** All tasks are finished, so every frame must
  be free and every PTBR -1. Anything else is a leak in `finalize()`.
- **Why are a task's pages scattered?** `getFreePage()` hands out the lowest free frame at
  the moment of each fault, and the 3 tasks fault in turn. Paging makes that harmless:
  each page is translated on its own, so a task never needs contiguous memory (no
  external fragmentation).
