# e19 — Gantt chart timeline of the scheduler

## Question (as it could appear in the test)
Instrument the Lab 4 scheduler. For every scheduler round, record which pid ran on each processor
(or that the processor was idle). When the OS exits, write `timeline.txt` with:
1. a text Gantt chart: one row per processor, one column per round, each cell the pid or `.` for idle;
2. for every task: arrival, start and finish round, and its waiting and turnaround time in rounds,
   with the averages;
3. the utilisation of every processor, and overall, in percent.
Run 5 tasks of known lengths on NP = 4, derive the chart by hand, and check that the file matches.

## Solution idea
- **Time unit.** A round is one pass of the scheduler loop in which every ready task gets one
  slice of `TIME_SLICE` (10) instructions. A round is counted only if some task is ready, so idle
  waits for shell input do not count and the chart is reproducible.
- **Recording.** At the start of each counted round, `round_no++` and `gantt` grows by `NP`
  zeros (one column, all processors idle). Before a task runs its slice on processor `p`:
  `gantt[(round_no-1)*NP + p] = pid`; `start` is set on its first slice; `run_rounds++`.
  `create_task` stores `arrival = round_no` (0 for command-line tasks), and `finish_task` stores
  `finish = round_no`.
- **Times.** `turnaround = finish - arrival`. `waiting = turnaround - run_rounds`, the rounds spent
  in the system without running; this is the textbook "turnaround minus burst". In this model a
  loaded task runs every round, so waiting = start - arrival - 1, but the general formula would
  still be right with context switching (e18). Utilisation = busy cells of a row / number of rounds.
- **Output.** `write_timeline()` writes the chart in blocks of `GANTT_COLUMNS` = 40 rounds, so long
  runs wrap. The cell width is the number of digits of the largest round number or pid, plus one.
  The function is called once, at the end of `scheduler()`.
- **Hand derivation.** `work.txt` sums N..1 and runs exactly 3N + 4 instructions. N = 2, 5, 8 and
  15 give 10, 19, 28 and 49 instructions, i.e. 1, 2, 3 and 5 rounds. The tasks in command-line
  order are pid 1 = 3 rounds, pid 2 = 1, pid 3 = 2, pid 4 = 5, pid 5 = 2.
  - Round 1: the loader gives P0..P3 to pids 1..4, and pid 5 waits. Pid 2 halts, which frees P1.
  - Round 2: `admit_waiting()` loads pid 5 on the first free processor, P1. Pid 3 halts (its 2nd
    round), which frees P2.
  - Round 3: pid 1 (3rd round) and pid 5 (2nd round) halt. P2 is idle because nobody is waiting.
  - Rounds 4-5: only pid 4 is left; it halts in round 5.
  ```
  round 1 2 3 4 5        busy: P0 3/5 = 60 %, P1 3/5 = 60 %, P2 2/5 = 40 %, P3 5/5 = 100 %
  P0    1 1 1 . .              all 13/20 = 65 %
  P1    2 5 5 . .        waiting: only pid 5, 1 round  -> average 1/5 = 0.20
  P2    3 3 . . .        turnaround: 3 1 2 5 3         -> average 14/5 = 2.80
  P3    4 4 4 4 4
  ```

## Changes
| file | function | change |
|---|---|---|
| config.h | — | `TIMELINE_FILE "timeline.txt"`, `GANTT_COLUMNS 40` |
| os.cpp | `struct Task` | `arrival`, `start`, `finish`, `run_rounds` (in rounds) |
| os.cpp | globals | `round_no` (counted rounds), `gantt` (NP pids per round, 0 = idle) |
| os.cpp | `create_task` | `arrival = round_no` |
| os.cpp | `finish_task` | `finish = round_no` |
| os.cpp | `write_timeline` (new) | chart in blocks of 40 rounds, per-task table and averages, utilisation per processor and overall |
| os.cpp | `scheduler` | new Gantt column per counted round; records pid per processor, first slice and run rounds; writes `timeline.txt` at exit |
| main.cpp | header comment | lists `timeline.txt` among the results |
| Makefile | `test`, `clean` | base tests and `./test.sh`; `clean` also removes `timeline.txt` |

## Run
```
cd minicomputer/extensions/e19_gantt_timeline
make
./sim -b work.txt work3.byte work.txt work1.byte work.txt work2.byte work.txt work5.byte work.txt work2.byte
cat timeline.txt                           # rewritten by every run (log.txt is appended to instead)
./test.sh                                  # 17 passed, 0 failed
SIM=$PWD/sim ../../tests/run_tests.sh      # 33 passed, 0 failed (scheduling is unchanged)
make clean                                 # also removes timeline.txt
```

## Expected output
Terminal:
```
[os] pid 1 created: work.txt work3.byte (7 instructions)
[os] pid 1 (work) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 2 created: work.txt work1.byte (7 instructions)
[os] pid 2 (work) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 3 created: work.txt work2.byte (7 instructions)
[os] pid 3 (work) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 4 created: work.txt work5.byte (7 instructions)
[os] pid 4 (work) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 5 created: work.txt work2.byte (7 instructions)
[os] pid 5 is waiting for a free processor/frames
[os] pid 2 (work) finished on processor 1 after 10 instructions, 0 page fault(s) -> work1_2.out, work_2.freq
[os] pid 5 (work) loaded on processor 1: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 3 (work) finished on processor 2 after 19 instructions, 0 page fault(s) -> work2_3.out, work_3.freq
[os] pid 1 (work) finished on processor 0 after 28 instructions, 0 page fault(s) -> work3_1.out, work_1.freq
[os] pid 5 (work) finished on processor 1 after 19 instructions, 0 page fault(s) -> work2_5.out, work_5.freq
[os] pid 4 (work) finished on processor 3 after 49 instructions, 0 page fault(s) -> work5_4.out, work_4.freq
[os] Gantt chart, task times and processor utilisation written to timeline.txt
[os] all tasks finished; print output is in log.txt
```
`timeline.txt`:
```
Gantt chart: pid that ran on each processor in each round ('.' = idle)
NP = 4 processors, TIME_SLICE = 10 instructions, 5 rounds

round 1 2 3 4 5
P0    1 1 1 . .
P1    2 5 5 . .
P2    3 3 . . .
P3    4 4 4 4 4

pid program          arrival start finish  run waiting turnaround
  1 work                   0     1      3    3       0          3
  2 work                   0     1      1    1       0          1
  3 work                   0     1      2    2       0          2
  4 work                   0     1      5    5       0          5
  5 work                   0     2      3    2       1          3
average waiting 0.20 rounds, average turnaround 2.80 rounds (5 tasks)

processor utilisation = busy rounds / 5 rounds
P0       3 / 5      60.0 %
P1       3 / 5      60.0 %
P2       2 / 5      40.0 %
P3       5 / 5     100.0 %
all     13 / 20     65.0 %
```
A 46-round task (`work.txt` with N = 150) wraps the chart after 40 rounds. The second block is:
```
round 41 42 43 44 45 46
P0     1  1  1  1  1  1
P1     .  .  .  .  .  .
P2     .  .  .  .  .  .
P3     .  .  .  .  .  .
```
`./test.sh` ends with `17 passed, 0 failed`. The base tests end with `33 passed, 0 failed`.

## Viva questions
**Q1. Why is P2 idle in round 3 while P0 and P1 are busy?**
Nobody is waiting: every remaining task already has its own processor. In the Lab 4 model a task
stays on one processor until it halts. Tasks never migrate, and one task cannot use two processors.

**Q2. Utilisation is only 65 %. Why, and how could it be higher?**
The long pid 4 runs alone in rounds 4-5, so 3 processors idle (the "tail"). Utilisation is higher
with more ready tasks than processors, or if the long job starts first (largest job first gives a
shorter makespan). Utilisation = busy cells / (rounds × NP) = 13 / 20.

**Q3. Define turnaround, waiting and response time.**
turnaround = finish - arrival. waiting = turnaround - run time (time ready but not running).
response = time until the first run. With rounds as the unit, pid 5 has turnaround 3, run 2, waiting 1.

**Q4. Why are idle loop passes (waiting for shell input) not counted as rounds?**
Their number depends on how fast the user types (wall-clock time), so the chart would change from
run to run. Counting only rounds in which a task runs makes the timeline deterministic.

**Q5. Why compute waiting as `turnaround - run_rounds` instead of `start - arrival - 1`?**
They are equal here only because a loaded task runs in every round. With context switching (more
tasks than processors, as in e18) a task can wait between slices too; the general formula still holds.

**Q6. How would you show the chart for 4 processors and 1000 rounds?**
In blocks: `GANTT_COLUMNS` (40) rounds per block, with the cell width taken from the largest
number. The file is rewritten at every exit, unlike log.txt, which is appended to.
