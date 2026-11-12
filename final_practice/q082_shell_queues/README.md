# Q082 — Shell command `queues`

## Question
Add a shell command `queues` that prints the ready queue and the waiting queue, front
first. It must show which loaded tasks get a slice in the next round, and in which order,
and which tasks still wait for a processor or for frames. Show both queues before and after
a new task is typed while all processors are busy.

## Answer
- **The two queues** are the `std::deque<int>`s of pids in `os.cpp`:
  - `ready_queue` holds the loaded tasks. Each owns a processor, and the next round serves
    them front to back.
  - `waiting_queue` holds tasks that were created but not loaded, because no processor was
    free or there were not enough free frames.
- **The command** is one more `if` in `run_command()`. A small lambda prints one queue, or
  `(empty)`. Printing does not change the queues.
- **The order.** Every round pops each ready task from the front and pushes it back at the
  end, unless it finished. So between rounds the queue is already in the next round's
  order. `loader()` appends a newly loaded task at the end.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `run_command` | `queues`: print `ready_queue` and `waiting_queue` front first, or `(empty)` |
| `os.cpp` | `run_command` | the usage message mentions `queues` |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` starts 6 tasks: `tiny.txt` (3 instructions) as pid 1, three counters and two
`counter_by100`, so pids 5 and 6 wait. The shell input comes from a file, so the shell runs
right after round 1:
1. `queues` → ready `2 3 4` (pid 1 finished in round 1), waiting `5 6`. Processor 0 is
   free, but `admit_waiting()` runs only at the start of a round or when a task is created.
2. `counter.txt counter_data.byte` → `create_task()` appends pid 7 and calls
   `admit_waiting()`. That loads pid 5 on processor 0; pids 6 and 7 stay.
3. `queues` → ready `2 3 4 5`, waiting `6 7`.

All 7 tasks then finish. With no tasks, both queues print `(empty)`. `sum_array` still
gives 45.

## Viva
- **Why is pid 5 not loaded as soon as pid 1 finishes?** Admission happens in `admit_waiting()`, which runs at the start of each round and on task creation, not in `finish_task()`. Calling it in `finish_task()` too would start pid 5 one round earlier.
- **Is the waiting queue strictly FIFO?** No. `admit_waiting()` skips a task that needs more frames than are free and may load a smaller one behind it. That uses memory well, but a big task can starve.
- **Where would a blocked task go?** A task waiting for I/O or a semaphore belongs in a third, blocked queue, not in the ready queue, so that it gets no slices until it is woken.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_by100.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`, `tiny.txt`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
