# e13 — Demand paging with page replacement

## Question (as it could appear in the test)
In the simulator a task stops with "no free physical frame" as soon as memory is full.
Implement page replacement: when no frame is free, choose a victim data page (FIFO by
default, LRU selectable with a `#define`), save it to a backing store, clear its
page-table entry and reuse the frame. Page it back in when it is used again. Decide
between local and global replacement and justify it. The loader should start a task as
soon as its instruction pages + 1 data page fit. Count page faults, page-ins and
page-outs per task. Run 4 tasks that overflow the 15 frames, with correct outputs.

## Solution idea
- **Backing store** (the "swap disk"): `backing_store[MAX_TASKS][NUM_DATA_PAGES][PAGESIZE]`
  in `os.cpp`, indexed by pid and logical data page. `swapped[pid-1][d]` = page d is not
  in memory but its contents are in the slot.
- **Frame table** `frame_info[NUM_PHYSICAL_PAGES]`: owner pid, logical page index,
  `loaded_at`, `last_used`. Page tables map page → frame. To pick a victim we need the
  reverse map (frame → page) and the ages.
- **Loader:** a task starts when its code pages + 1 frame are free (not all its pages).
  `initialize()` loads the code pages and data page 0. Data pages 1.. of the data file go
  to the backing store (`swapped = true`) and are paged in on first use. So the task
  always owns at least one data frame.
- **Page fault** (`page_fault()`, called by `getPhysicalAddress()` for an unmapped data page):
  1. take a free frame. If there is none, `replace_page()` picks a victim, and
     `page_out()` copies it to the backing store and clears its page-table entry (the
     owner's next access faults);
  2. fill the frame: copy the slot back if the page is swapped (**page-in**), else zero it
     (first touch, as in Lab 5);
  3. write the page-table + frame-table entry and count the fault.
- **Victim:** only DATA pages. Code pages are pinned: every fetch needs them and they are
  never written. FIFO = smallest `loaded_at`. LRU = smallest `last_used`, which
  `getPhysicalAddress()` refreshes on every access (`REPLACE_LRU 1`).
- **Local replacement (chosen, `GLOBAL_REPLACEMENT 0`):** the victim is one of the faulting
  task's own pages.
  - (1) Isolation: a task that thrashes cannot steal other tasks' frames, so each task's
    fault count depends only on itself.
  - (2) Always a victim: the task keeps the data frame the loader gave it.
  - (3) Only this processor's page table changes. Global replacement must edit another
    processor's table while that task is running; real hardware then needs a TLB shootdown.
  - Price: frames are first-come-first-served and stay with a task until it ends.

  `GLOBAL_REPLACEMENT 1` is implemented too (any task's oldest data page).
- **finalize():** each output byte comes from the frame if the page is resident, else
  from the backing store if swapped, else 0. It then frees the frames, the frame-table
  entries and the slots. It prints
  `paging (local FIFO): F faults = Z zero-fill + I page-in, O page-out, D data frame(s) at the end`.

**Expected numbers: 4 tasks** (`pagewalk.txt`, K = 6 pages, R = 3 passes, 221 instructions each)
- **Setup:** each task needs 1 code + 7 data pages, so 32 frames for 15. The loader uses
  4 x 2 frames, which leaves **7 free**.
- **Lockstep:** the fill loop is 9 instructions, and page p is first touched by
  instruction 9p + 1. So each task touches one new page per round (10-instruction slices),
  and the 4 identical tasks run in lockstep:

  | round | pid 1 | pid 2 | pid 3 | pid 4 | free |
  |---|---|---|---|---|---|
  | 1 | page 1 (free frame, page-in from the file) | same | same | same | 3 |
  | 2 | page 2 (free) | page 2 (free) | page 2 (free) | page 2: **no frame** → evicts its own page 0 | 0 |
  | 3-6 | evicts its oldest page (0, 1, 2, 3) | same | same | evicts 1, 2, 3, 4 | 0 |

- **After the fill:** pids 1-3 hold pages {4, 5, 6} and pid 4 holds {5, 6}. Each pass
  reads pages 1..6 in a cycle longer than the resident set. The page needed next is
  always the oldest one, so **every read faults**.
- **Faults** = 6 (fill: 1 page-in from the data file + 5 zero-fill) + 3 x 6 = **24**.
  Page-ins = 1 + 18 = **19**.
- **Page-outs** = faults - free frames taken: 24 - 2 = **22** (pids 1-3), 24 - 1 = **23** (pid 4).
  Summed over all tasks: faults - page-outs = 7, the free frames.
- **Why no later surprises:** the final store goes to page 6, which is still resident,
  and the HALT is alone in the last slice. So no frame freed by a finished task changes
  the other tasks' counts.
- **Correctness:** page 0 of every task was paged out in round 2-3 and never came back.
  The output still shows K, R, S there, read from the backing store.

**Expected numbers: FIFO vs LRU** (`-DMEMSIZE=2048`: 3 frames = code + 2 data frames, one task)
- **pagewalk:** the reads form a pure cycle of 6 pages over 2 frames. FIFO and LRU both
  evict exactly the page needed next, so both give 24 faults. LRU is no help for a
  cyclic pattern larger than memory.
- **hotpage:** the running sum lives in page 0, so the reference string per pass is
  `1 0 2 0 3 0 4 0 5 0 6 0`.
  - **FIFO** evicts page 0 whenever it is the oldest page, about every other read:
    `[0,6] 1→[6,1] 0→[1,0] 2→[0,2] 0 3→[2,3] 0→[3,0] 4→[0,4] 0 5→[4,5] 0→[5,0] 6→[0,6] 0`
    = 9 faults per pass, total 6 + 3 x 9 = **33**.
  - **LRU:** page 0 was always used just before, so the victim is always the previous
    page p. 6 + 7 (page 0 comes back once) + 6 + 6 = **25**.
- **big_data.byte** (3-page data file): the base simulator refuses it on a 3-frame machine
  ("task needs more frames than physical memory has"). e13 runs it with 2 data frames.

## Changes
| File | Function | Change |
|---|---|---|
| `config.h` | | `MEMSIZE` overridable (`-DMEMSIZE=2048`); `REPLACE_LRU 0`, `GLOBAL_REPLACEMENT 0` |
| `os.cpp` | (globals) | `FrameInfo frame_info[]` (frame table), `mmu_clock`, `backing_store[][][]`, `swapped[][]` |
| `os.cpp` | `struct Task` | `page_ins`, `page_outs` |
| `os.cpp` | `map_page()` | new: write the page-table entry and the frame-table entry (owner, page, time) |
| `os.cpp` | `page_out()` | new: frame → backing store, clear the owner's page-table entry, count |
| `os.cpp` | `replace_page()` | new: victim = oldest `loaded_at` (FIFO) / `last_used` (LRU) data frame, own (local) or any (global) |
| `os.cpp` | `page_fault()` | new: free frame or replacement; page-in or zero-fill; map; count; `-t` traces it |
| `os.cpp` | `getPhysicalAddress()` | unmapped data page → `page_fault()`; every access updates `last_used` |
| `os.cpp` | `copy_in()` | uses `map_page()` |
| `os.cpp` | `initialize()` | only data page 0 gets a frame; data pages 1.. go to the backing store |
| `os.cpp` | `finalize()` | non-resident pages from the backing store; frees frame table and slots; paging report |
| `os.cpp` | `loader()`, `create_task()` | need code pages + 1 frame instead of all pages |
| `Makefile` | `test` | runs `./test.sh`, then the 33 base checks |

## Run
```bash
make
./sim -b pagewalk.txt pw1_data.byte pagewalk.txt pw2_data.byte pagewalk.txt pw3_data.byte pagewalk.txt pw4_data.byte
./sim -b -t hotpage.txt pw1_data.byte 2>&1 | grep mmu       # every page-in / page-out
make clean && make CXXFLAGS="-std=c++17 -O2 -Wall -Wextra -DMEMSIZE=2048 -DREPLACE_LRU=1"   # 2 KB + LRU
make clean && make
./test.sh                                    # 20 checks (builds the 2 KB FIFO/LRU and global variants itself)
SIM=$PWD/sim ../../tests/run_tests.sh        # 33 passed: the base examples never overflow memory
```
`pw<S>_data.byte`: K = 6, R = 3, S = 1..4, word 512 = 1000. `big_data.byte`: K = 6, R = 3,
S = 5, words 512 = 1000 and 1024 = 2000.

## Expected output
```
$ ./sim -b pagewalk.txt pw1_data.byte pagewalk.txt pw2_data.byte pagewalk.txt pw3_data.byte pagewalk.txt pw4_data.byte
[os] pid 1 (pagewalk) loaded on processor 0: 1 instruction + 1 data page(s), 1 more in the backing store, 13 frame(s) left
[os] pid 2 (pagewalk) loaded on processor 1: 1 instruction + 1 data page(s), 1 more in the backing store, 11 frame(s) left
[os] pid 3 (pagewalk) loaded on processor 2: 1 instruction + 1 data page(s), 1 more in the backing store, 9 frame(s) left
[os] pid 4 (pagewalk) loaded on processor 3: 1 instruction + 1 data page(s), 1 more in the backing store, 7 frame(s) left
[os] pid 1 (pagewalk) finished on processor 0 after 221 instructions, 24 page fault(s) -> pw1_data_1.out, pagewalk_1.freq
[os] pid 1 paging (local FIFO): 24 faults = 5 zero-fill + 19 page-in, 22 page-out, 3 data frame(s) at the end
[os] pid 2 (pagewalk) finished on processor 1 after 221 instructions, 24 page fault(s) -> pw2_data_2.out, pagewalk_2.freq
[os] pid 2 paging (local FIFO): 24 faults = 5 zero-fill + 19 page-in, 22 page-out, 3 data frame(s) at the end
[os] pid 3 (pagewalk) finished on processor 2 after 221 instructions, 24 page fault(s) -> pw3_data_3.out, pagewalk_3.freq
[os] pid 3 paging (local FIFO): 24 faults = 5 zero-fill + 19 page-in, 22 page-out, 3 data frame(s) at the end
[os] pid 4 (pagewalk) finished on processor 3 after 221 instructions, 24 page fault(s) -> pw4_data_4.out, pagewalk_4.freq
[os] pid 4 paging (local FIFO): 24 faults = 5 zero-fill + 19 page-in, 23 page-out, 2 data frame(s) at the end
[os] all tasks finished; print output is in log.txt

$ ./test.sh
e13: 4 tasks x (1 code + 7 data pages) = 32 frames on 15 frames (local FIFO, 8 KB)
  ok    loader: each task starts with code + 1 data page, page 1 waits in the backing store
  ok    all 4 sums R * (1000 + K*S + 21) correct (at 6*512 + 4)
  ok    pid 2: page 0 (paged out since round 3) comes back from the backing store: K, R, S = 6,3,2
  ok    pid 2: page 1 = 1000 + S + 1, page 3 = S + 3, page 6 = S + 6
  ok    pids 1-3: 24 faults = 5 zero-fill + 19 page-in, 22 page-out, 3 data frames
  ok    pid 4 (found no free frame in round 2): 24 faults, 23 page-out, 2 data frames
  ok    faults - page-outs over all tasks = 7 frames free when the tasks started
e13: 2 KB memory (3 frames: code + 2 data frames), local FIFO
  ok    a task with a 3-page data file starts (base: 'needs more frames than physical memory has')
  ok    big_data: sum 3 * (1000 + 2000 + 6*5 + 21) = 9153, page 2 = 2000 + 5 + 2
  ok    big_data: 24 faults = 4 zero-fill + 20 page-in (pages 1 and 2 from the file), 23 page-out
  ok    hotpage FIFO: sum 3081 kept in page 0
  ok    hotpage FIFO: 33 faults = 6 (fill) + 3 passes x 9
e13: 2 KB memory, local LRU (-DREPLACE_LRU=1)
  ok    hotpage LRU: same sum 3081
  ok    hotpage LRU: 25 faults (page 0 stays resident)
  ok    pagewalk (pure cycle of 6 pages > 2 frames): LRU = FIFO, every read faults
  ok    pagewalk LRU sum 3081
e13: global replacement (-DGLOBAL_REPLACEMENT=1), the same 4 tasks
  ok    global: all 4 sums correct
  ok    global: pid 3 page 0 back from the backing store (K, R, S = 6,3,3)
  ok    global: pid 1 lost a frame to pid 4 (24 faults, 23 page-out, 2 frames); pids 2-4 have 3
  ok    global: faults - page-outs = 7

20 passed, 0 failed
```
- **The base simulator on the same jobs:** with 8 KB, 3 of the 4 tasks stop with
  `ERROR: no free physical frame for data page ...`. Only pid 2 finishes, on the frames the
  failed tasks gave back. On the 2 KB machine it refuses `big_data.byte` at load time.
- **Global replacement here:** in round 2, pid 4 takes pid 1's page 0, the globally
  oldest data page. After that each task evicts the next task's oldest page. The totals
  are the same as with local replacement, because the 4 tasks are identical. Only *who*
  ends up with 2 frames changes (pid 1 instead of pid 4).

## Viva questions
- **Why are code pages never replaced?** Every fetch uses them, so they would fault right
  back. They are never written, so they would never need a page-out (the program file is
  their backing store). Pinning them also keeps the fetch path free of page faults.
- **Why do FIFO and LRU give the same result for pagewalk?** In a cycle longer than the
  number of frames, the page needed next is both the oldest and the least recently used.
  Both policies evict it, so every access faults. OPT (evict the page used farthest in
  the future) would keep some pages and fault less.
- **What is thrashing here, and how do you stop it?** The resident set (2-3 pages) is
  smaller than the working set (6 pages), so every read faults. Fixes: give the task
  more frames (working-set model / page-fault-frequency) or admit fewer tasks (load
  control in the loader).
- **Belady's anomaly?** With FIFO, more frames can give *more* faults (reference string
  `1 2 3 4 1 2 5 1 2 3 4 5`: 9 faults with 3 frames, 10 with 4). LRU is a stack algorithm
  and cannot show it.
- **How would a dirty bit help?** A page not written since its page-in still has an equal
  copy in the backing store, so it can be dropped without a page-out. Here every eviction
  writes the page (22 page-outs). e14 adds DIRTY to the page-table entry.
- **Local vs global in one line?** Local isolates tasks and needs no cross-processor page-table
  (TLB) changes. Global uses memory better when tasks have different working-set sizes,
  but one task can steal frames from the others.
