# Q069 — Share instruction frames between tasks running the same program

## Question
When several tasks run the same program, each one gets its own copy of the code. Change
the loader so that a task whose byte code is identical to that of a running task maps
that task's instruction frames instead of loading a copy. Keep a reference count per
frame, and free a frame only when its last user finishes. Why is it safe to share
instruction pages but not data pages? Show the frames saved and that results are correct.

## Answer
- **`frame_refs[NUM_PHYSICAL_PAGES]`** counts how many page-table entries point at each
  frame. `getFreePage()` sets it to 1.
- **`same_code_on(t)`** returns a processor that runs a task whose `prog.code` equals
  `t.prog.code` (byte-for-byte), or -1. It compares the code, not the file name, so two
  names for the same program share, and a changed file does not.
- **`loader()`**: if a sharer exists, the task needs only its data-file frames
  (`need` = 0 code pages + data pages). It passes the sharer to `initialize()`.
- **`initialize(p, t, share)`**: instead of `copy_in` for the code, it copies the sharer's
  instruction entries into the new table and increments `frame_refs` of each frame.
- **`finalize()`**: `if (frame && --frame_refs[frame] == 0) freePages[frame] = 0;`. A frame
  that still has users is left alone, and a message says so.

**Why it is safe.** Instruction pages are never written: stores always go to data
pages (`isFetch = 0`). Every sharer therefore sees exactly the bytes it would have loaded
itself. Data pages are written, so sharing them would let one task change another's
variables.

Run `sum_array`/`sum_n_data` (pid 1, 43 instructions), `sum_array` (pid 2, 71), `counter`
(pids 3 and 4, 43 each) and `pages` (pid 5, waits):
```
pid 1 loaded ... 13 frame(s) left          frames 1 (code), 2
pid 2 shares 1 instruction frame(s) with pid 1  -> 12 left   (data 3 only)
pid 3 loaded ... 10 left                   frames 4 (code), 5
pid 4 shares 1 instruction frame(s) with pid 3  ->  9 left   (data 6 only)
round 5: pid 1: frame 1 is still used by 1 other task(s)
         pid 3: frame 4 is still used by 1 other task(s);  pid 4 ends: frame 4 freed
round 6: pid 5 (pages) loads: 15 - 2 (pid 2) - 2 = 11 left, on frames 2 and 4
```
- **Frames saved:** the 4 tasks use 6 frames instead of 8.
- **The reference count matters:** if pid 1 had freed frame 1, `pages` would have been
  loaded into it in round 6, over the code that pid 2 is still running. That variant
  makes pid 2 print 9 instead of 45.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | (globals) | `int frame_refs[NUM_PHYSICAL_PAGES]` |
| `os.cpp` | `getFreePage` | `frame_refs[f] = 1` |
| `os.cpp` | `same_code_on` (new) | processor running identical byte code, or -1 |
| `os.cpp` | `initialize` | new parameter `share`: map the sharer's instruction frames, `frame_refs++` |
| `os.cpp` | `loader` | `share = same_code_on(t)`; no code frames needed when sharing |
| `os.cpp` | `finalize` | free a frame only when `--frame_refs[frame] == 0` |

The exact code is in [`solution.patch`](solution.patch).

## Test
`./test.sh` runs the 5 tasks above and checks:
- the two `shares` lines (and no others);
- the frames left after each load: 13 12 10 9 and later 11;
- the two `still used` lines (and none for the last users);
- the 5 results: 99, 45, 10, 10, 21.

It also checks that `sum_array` alone gives 45.

## Viva
- **What do real systems call this?** Shared text segments: every process running the same executable (or shared library such as libc) maps the same physical frames read-only.
- **What if code could be written (self-modifying code)?** Then the frames would have to be marked read-only and copied on the first write: copy-on-write, as `fork()` does for data pages.
- **Why reference counts and not "free when the owner ends"?** There is no owner: any sharer can finish first. Here pid 1 loaded the code but finished first (43 instructions) while pid 2 (71) still needed it.
- **Why compare the byte code and not the file name?** The name does not prove that the content is the same: the file may have been edited between the two task starts. Equal bytes are exactly the condition for sharing.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_data.byte`, `pages.txt`, `pages_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `sum_n_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
