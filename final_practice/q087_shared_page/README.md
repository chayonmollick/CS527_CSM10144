# Q087 — A shared memory page

## Question
Give the tasks a shared page. Data addresses 3584..4095 (data page 7) of every task
must map to ONE physical frame, so a value one task stores there can be read by another
task. The rest of the data space stays private. The shared page must survive when a task
that used it finishes. Demonstrate it with a writer task that stores values and a reader
task that reads them later. Make the order deterministic: the reader waits on a flag word.

## Answer
- **Shared memory = two page tables pointing at the same frame.** So
  `getPhysicalAddress()` does not change at all. Only the OS fills the page tables
  differently:
  - `os_init()` takes one frame from the free list at start-up (frame 1) and keeps it
    in `shared_frame`. Memory was just zeroed, so the shared page starts as zeros.
  - `initialize()` loads the code and the data file as before. Then it sets the
    page-table entry of data page 7 (index 2 + 7 = 9) to `shared_frame`, so the page is
    mapped from the start and never page-faults.
  - `finalize()` releases the task's frames but skips `shared_frame`, because other
    tasks, including tasks that start later, still use it. It still writes page 7 into
    the writer's `.out`.
  - `create_task()`: the data file may now be at most 3584 bytes. Otherwise the loader
    would put the end of the file into a private page 7 that `initialize()` then
    overwrites, and that frame would be lost.
- **Cost:** tasks now have 14 frames instead of 15. The first loaded line says
  `12 frame(s) left` instead of 13.
- **Side effect:** every program that uses addresses ≥ 3584 now shares them with the other
  tasks. `examples/pages.txt` writes to 3825, so it now has 2 page faults instead of 3.
  Its sum is still 21. Lab6's `make test` with this build reports that one difference.
- **Demo** (reader = pid 1, writer = pid 2; the ready queue runs pid 1 first in every round):
  1. Round 1: the reader polls the flag (0); the writer stores 10, 20, 30.
  2. Round 2: the reader polls first and the flag is still 0. Then the writer sets the
     flag, stores its private word and finishes, and its private frames are freed.
  3. Round 3: the reader sees the flag and reads 10 + 20 + 30 = 60, although the writer has already ended.
- **Proof that the frame was kept:** before reading, the reader writes to its own page 1,
  and that page fault takes the lowest free frame. If `finalize()` had freed frame 1
  together with the writer's frames, the reader would get it, zeroed, and the sum would
  be 0. We checked this by removing the `!= shared_frame` test.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | globals | `SHARED_PAGE = NUM_DATA_PAGES - 1` (data page 7), `shared_frame` |
| `os.cpp` | `os_init` | `shared_frame = getFreePage()` (frame 1, never freed) |
| `os.cpp` | `initialize` | page-table entry of data page 7 = `shared_frame` |
| `os.cpp` | `finalize` | do not put `shared_frame` back on the free list |
| `os.cpp` | `create_task` | a data file must end below the shared page (≤ 3584 bytes) |

## Test
`./test.sh` runs `reader.txt` (pid 1) and `writer.txt` (pid 2) together:
- **Values:** the reader stores 60; the writer's private word 0 is 7; the writer's output shows 10 at 3584 and the flag 1 at 3596.
- **Order:** the writer finished before the reader.
- **Frames:** 12 frames are left after the first load. The writer has 0 page faults (page 7 is premapped); the reader has 1 (its private page).
- **Data file limit:** a 3588-byte data file is refused.
- **Old programs:** `sum_array` still gives 45.

## Viva
- **How do real systems share memory?** Two page tables point at the same frame (`shmget`/`shmat`, `mmap` with `MAP_SHARED`). The logical addresses may differ between processes; here both use the same page for simplicity.
- **When may a shared frame be freed?** Only when nobody maps it any more. A real OS keeps a reference count per frame. Here the frame lives until the simulator stops.
- **Is the flag safe?** The writer stores the data before the flag, and the simulator executes one instruction at a time, so the reader never sees the flag without the data. Real CPUs reorder memory accesses and need barriers. With several writers you need a lock (semaphore, Q086).
- **Why does the reader cost 36 instructions?** 21 of them are polling: this is busy waiting. Blocking (Q086 `wait`) would cost nothing while waiting.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `reader.txt`, `shm_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `writer.txt`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
