# Q089 — Priority scheduling with aging

## Question
Give every task a priority, written after its data file on the command line or in the
shell (`prog data p`, default 0, higher = more important). When a processor becomes
free, the waiting task with the highest priority gets it; on a tie, the one that has
waited longest. To prevent starvation, every waiting task gains 1 priority per scheduler
round it spends waiting (aging). Print each task's priority when it starts. Show that an
old low-priority task can overtake a newer task that had a higher priority.

## Answer
- **Where a priority can act in lab6.** Every loaded task gets a slice in every round
  and keeps its processor until it finishes. There is no preemption, so the policy only
  decides **who gets a processor when one becomes free**. That decision is made in
  `admit_waiting()`.
- **Setting the priority:**
  - `Task.priority` is set by a new `create_task()` parameter (default 0).
  - `parse_priority()` accepts "0".."9999". `main()` and `run_command()` use it to see
    whether a third word follows the data file.
  - `main()` first checks the whole argument list, as lab6 did with its even-count test.
- **Choosing a task.** `admit_waiting()` sorts the waiting queue by priority, highest
  first, with `std::stable_sort`; the old loader loop runs after it unchanged.
  - A stable sort keeps equal priorities in queue order, so ties go to the task that
    has waited longest.
  - The loop still tries every waiting task, so if the best one does not fit in the free
    frames, a smaller one can start.
- **Aging.** After every round's slices, each task still in the waiting queue gets +1.
  This happens before `shell()`, so a task typed during that shell call is not credited
  with a round it did not wait. The priority a task starts with = its own + rounds
  waited, printed as `[os] pid N starts with priority P`.
- **Scenario A** (all tasks arrive together). Jobs of 2, 4, 6 and 6 rounds hold the
  processors; pids 5, 6 and 7 (priorities 1, 4, 2) wait.
  1. Round 3 starts pid 6 (4 + 2 = 6). It finishes in that round.
  2. Round 4 starts pid 7 (2 + 3 = 5).
  3. Round 5 starts pid 5 (1 + 4 = 5).
- **Scenario B** (aging). Pid 5 (priority 0) waits from the start. Pid 6 (priority 1)
  is typed in the shell after round 1, so it is one round younger.
  - When pid 1 finishes after round 4, both have 4: 0 + 4 for pid 5 and 1 + 3 for pid 6.
    The tie goes to the older pid 5.
  - Without aging, pid 6 starts first (checked by deleting the aging line).

## Changes
| File | Function | Change |
|---|---|---|
| `main.cpp` | `usage`, `main` | tasks are `prog data [priority]`: check the whole list, then create each task with its priority |
| `os.h/.cpp` | `create_task`, `parse_priority` | new `priority` parameter (default 0) stored in the task; `parse_priority` accepts "0".."9999" |
| `os.cpp` | `struct Task` | `int priority` |
| `os.cpp` | `loader` | prints `pid N starts with priority P` |
| `os.cpp` | `admit_waiting` | `stable_sort` of the waiting queue, highest priority first |
| `os.cpp` | `run_command` | accepts `<program> <data> <priority>` |
| `os.cpp` | `scheduler` | after each round: `priority++` for every waiting task |

## Test
`./test.sh`:
- **Scenario A (batch):** the tasks start as `6:6 7:5 5:5` (pid:priority), and all 7 finish.
- **Scenario B:** the shell line comes from a file, so it is read in the first `shell()` call, right after round 1. The tasks start as `5:4 6:5`.
- **Usage:** a program without its data file is refused.
- **Old programs:** `sum_array` still gives 45.

## Viva
- **Why does the priority not preempt a running task?** In lab6 a task's registers live in its processor's arrays and there is no PCB, so a running task cannot be paused. The priority is therefore long-term scheduling (admission). Preemption needs a context switch (extension e18).
- **What does aging fix?** Starvation. Without it, a steady stream of high-priority arrivals keeps a low-priority task waiting forever. With +1 per round, a task with a gap of d reaches the top after at most d rounds plus the wait for a free processor.
- **Why `stable_sort` and not `sort`?** `std::sort` may reorder equal elements. `stable_sort` keeps equal priorities first-come first-served.
- **What else could the priority change?** The order of the ready queue inside a round. In lab6 that changes only the order of `print` lines, because every loaded task runs every round anyway.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `r1.byte`, `r2.byte`, `r4.byte`, `r6.byte`, `spin.txt`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
