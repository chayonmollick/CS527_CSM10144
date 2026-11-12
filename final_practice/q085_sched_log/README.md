# Q085 — Log every scheduling decision to `sched.log`

## Question
Make the scheduler write a file `sched.log` with one line for every time slice it hands
out: the round number, the pid, the processor, and the number of instructions the task
actually ran in that slice, which is fewer than the time slice when the task halts. Each run
starts a new file. Use the file to check by hand how 5 counters share 4 processors.

## Answer
- **Where the decisions are made.** In `scheduler()`, each round takes the next pid from the
  ready queue and calls `process_instructions(p, TIME_SLICE)`. That function already returns
  how many instructions ran: it stops early at a halt or an error. One `fprintf` after the
  call records the decision.
- **The round number** is a local counter. It goes up at the start of every round in which
  some task runs (`n > 0`); idle passes, while the shell waits for input, are not counted.
- **The file** is opened with `"w"` when the scheduler starts, so each run starts a fresh log
  (`log.txt`, by contrast, appends). It gets a header line, and it is closed at the end.

## Changes
| File | Function | Change |
|---|---|---|
| `os.cpp` | `scheduler` | open `sched.log` (`"w"`) and write a header; `int round` counter; `int ran = process_instructions(...)`; one `fprintf` per slice; `fclose` at the end |

The exact code is in [`solution.patch`](solution.patch). Apply it to a copy of `lab6` with
`patch -p1 < solution.patch`.

## Test
`./test.sh` runs 5 counters in batch mode. Pids 1, 3 and 5 are `counter` (43
instructions) and pids 2 and 4 are `counter_by100` (54). By hand:
- **Rounds 1–4:** pids 1–4 on processors 0–3, 10 instructions each (16 lines).
- **Round 5:** `1@0:3` (halts), `2@1:10`, `3@2:3` (halts), `4@3:10`.
- **Round 6:** `admit_waiting()` loads pid 5 on processor 0, the first free one, at the end
  of the ready queue: `2@1:4`, `4@3:4`, `5@0:10`.
- **Rounds 7–9:** pid 5 runs 10 each. **Round 10:** pid 5 runs 3.

So the file has 27 lines after the header, 237 instructions in all, and 43, 54, 43, 54, 43
per pid. The last line is `10 5 0 3`. A second run gives 27 lines again, because the file
is rewritten. `sum_array` alone gives 8 slices and 71 instructions, and still 45.

## Viva
- **Why log in the scheduler, not in the processor?** The scheduler makes the decision: who runs, where and for how long. The processor only executes the instructions it is given.
- **What can be computed from the log?** A Gantt chart per processor, each task's turnaround and waiting time (Q081), and the processor utilisation. Here 27 slices fill 27 of the 40 processor-rounds (10 rounds × 4 processors), and processors 1–3 are idle in rounds 7–10.
- **Why `"w"` and not `"a"`?** One run gives one log, so the round numbers start at 1 and never mix with an older run.

## Files
- `solution/`: the complete simulator with this answer applied (lab6 + `solution.patch`).
  Build and run it directly: `cd solution && make`, then `./sim -b ../<program>.txt ../<data>.byte`.
- `solution.patch`: only the changed lines, to apply to your own copy: `patch -p1 < solution.patch`.
- Inputs (programs and data): `counter.txt`, `counter_by100.txt`, `counter_data.byte`, `sum_array.txt`, `sum_array_data.byte`.
- `test.sh`: applies the answer to a fresh copy of lab6, builds it and checks the results.
