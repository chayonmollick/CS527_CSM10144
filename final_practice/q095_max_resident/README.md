# Q095 — Option `-k K`: at most K tasks resident

## Question
Add a command-line option `-k K` (1 ≤ K ≤ NP) that limits how many tasks are resident
in memory at the same time to K, even when more processors are free. The other tasks
wait in the waiting queue and start as resident tasks finish. Without `-k` the simulator
must behave exactly as before. Show 4 tasks with `-k 2` and 3 tasks with `-k 1`.

## Answer
- **What "resident" means in lab6.** A task is loaded into frames only when it also
  gets a processor, and it keeps both until it finishes. So the number of resident
  tasks is the number of busy processors (`proc_owner[p] != 0`).
- **Where to limit it.** The limit is the *degree of multiprogramming*. A task becomes
  resident in only one place, `loader()`. It now counts the busy processors and refuses
  when `resident >= max_resident`. The task then stays in the waiting queue, exactly as
  when no processor or not enough frames are free, and `admit_waiting()` tries it
  again at the start of every round.
- **The option.** `max_resident` is a global in `os.cpp`, declared in `os.h`, with
  default NP so that the old behaviour is unchanged. `main()` sets it from
  `-k K` with `atoi`. A value outside 1..NP prints the usage and exits with 1.
  `<cstdlib>` is included for `atoi`.
- **With `-k 2`** (print3 = 15 instructions = 2 slices):
  - Pids 1 and 2 load on processors 0 and 1. Pids 3 and 4 wait, although processors 2 and 3 are free.
  - Rounds 1–2: pids 1 and 2 print and finish.
  - Round 3: pids 3 and 4 load on the lowest free processors, 0 and 1.

  `log.txt`: `1 1 2 2 1 2 3 3 4 4 3 4`.
- **With `-k 1`:** one task at a time, like a uniprogrammed batch system. `log.txt`: `1 1 1 2 2 2 3 3 3`.

## Changes
| File | Function | Change |
|---|---|---|
| `main.cpp` | includes, `usage`, `main` | `<cstdlib>`; option `-k K` sets `max_resident`, checked 1..NP |
| `os.h/.cpp` | `max_resident` | new global, default NP |
| `os.cpp` | `loader` | count the loaded tasks; refuse when `resident >= max_resident` |

## Test
`./test.sh`:
- **`-k 2`, 4 tasks:**
  - Two tasks wait, and processors 2 and 3 are never used.
  - `log.txt` shows `1 1 2 2 1 2 3 3 4 4 3 4`, and all four outputs are 3.
- **`-k 1`, 3 tasks:** `1 1 1 2 2 2 3 3 3`.
- **No `-k`:** `1 1 2 2 3 3 4 4 1 2 3 4`, as in lab6.
- **Errors:** `-k 0` and `-k 5` are refused (exit code 1).
- **Old programs:** `sum_array` still gives 45.

## Viva
- **Why keep processors idle?** To save memory. Each resident task holds frames for its code, data and demand-paged pages. With swapping, too many resident tasks cause thrashing. Real systems control the degree of multiprogramming with the long-term scheduler, and the medium-term scheduler swaps tasks out.
- **What does `-k 1` cost?** The 3 tasks take 6 rounds instead of 2, and the last one waits 4 rounds before it starts. The turnaround time grows because nothing overlaps.
- **Why count `proc_owner` and not `ready_queue.size()`?** Between rounds both give the same number. `proc_owner` is also right in the middle of a round, while the ready queue is being rotated, so the test cannot go wrong if `loader()` is ever called from there.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `print3.txt`, `print3_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
