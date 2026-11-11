# e16 — Priority and shortest-job-first scheduling

## Question (as it could appear in the test)
Extend the Lab 4 OS so that the scheduling policy is chosen by `SCHED_POLICY` in `config.h`:
`ROUND_ROBIN` (the current behaviour), `PRIORITY` and `SJF`.
- PRIORITY: the shell and the command line take an optional priority (`prog.txt data.byte 3`).
  Higher-priority tasks run first in every round, equal priorities are served round robin, and
  the waiting queue is also kept in priority order.
- SJF: shortest job first, where the job length is the program's instruction count. Non-preemptive.
- At exit, print each task's arrival, first-run and finish round, its waiting and turnaround time
  in rounds, and the averages. Use 6 tasks on NP = 4 processors to show how the finish order changes.

## Solution idea
- **What a policy can change here.** The Lab 4 model gives each task its own processor until it
  finishes, so every loaded task already gets one slice per round. A policy can therefore decide
  only two things: (1) **which waiting task gets a processor when one becomes free** (long-term
  scheduling, the waiting-queue order), and (2) the order of the tasks inside a round (the
  ready-queue order). It cannot take a processor away from a running task, because a task's
  registers live in its processor's arrays and there is no PCB or context switch (that is e18).
  So both PRIORITY and SJF are non-preemptive.
- **One comparison, two sorted queues.** `goes_before(a, b)` returns `a.priority > b.priority` for
  PRIORITY, `a.length < b.length` for SJF, and always `false` for ROUND_ROBIN.
  `insert_by_policy(q, pid)` puts a task behind every task that goes before it **or ties with it**,
  so equal tasks stay in arrival order. That gives round robin among equal priorities, and plain
  FIFO for ROUND_ROBIN. The same function keeps the waiting queue (in `create_task`) and the ready
  queue (in `loader`) sorted. The round loop pops every task and appends it again, so a round
  never breaks the sorted order.
- **Admission is deferred to the scheduler.** In the base, `create_task` loads a task at once, so
  the first 4 command-line tasks would get processors in typing order before any policy could act.
  Now `create_task` only queues the task, and `admit_waiting()` loads tasks in policy order at the
  start of the next round. All tasks that arrive together therefore compete for the processors.
- **Metrics, in rounds.** A round is one slice of `TIME_SLICE` instructions for every ready task,
  and only rounds in which something runs are counted. `arrival` is the round counter when the
  task is created (0 for command-line tasks), `start` is the round of its first slice, and
  `finish` is the round in which it halts. `waiting = start - arrival - 1`, because once it has a
  processor a task never waits again. `turnaround = finish - arrival`.
- **Hand derivation.** The jobs are straight-line code, so their length is exact: short = 10
  instructions (1 round), mid = 30 (3 rounds), long = 50 (5 rounds). All arrive in round 0.

  | pid | 1 | 2 | 3 | 4 | 5 | 6 |
  |---|---|---|---|---|---|---|
  | job (rounds) | long 5 | mid 3 | long 5 | short 1 | mid 3 | short 1 |
  | priority | 1 | 2 | 3 | 1 | 5 | 4 |

  - RR: round 1 loads 1 2 3 4, and 4 finishes. Round 2 loads 5. In round 3, 2 finishes. Round 4
    loads 6, and 5 and 6 finish. In round 5, 1 and 3 finish. **Order 4 2 5 6 1 3.**
  - PRIORITY: round 1 loads the 4 highest priorities, 5 6 3 2, and 6 finishes. Round 2 loads 1
    (priority 1, which arrived before 4). In round 3, 5 and 2 finish. Round 4 loads 4, which
    finishes at once. In round 5, 3 finishes; in round 6, 1 finishes. **Order 6 5 2 4 3 1.**
  - SJF: round 1 loads the 4 shortest, 4 6 2 5, and 4 and 6 finish. Round 2 loads 1 and 3. In
    round 3, 2 and 5 finish. In round 6, 1 and 3 finish. **Order 4 6 2 5 1 3.**

  | policy | finish order | avg waiting | avg turnaround |
  |---|---|---|---|
  | ROUND_ROBIN | 4 2 5 6 1 3 | 4/6 = 0.67 | 22/6 = 3.67 |
  | PRIORITY | 6 5 2 4 3 1 | 4/6 = 0.67 | 22/6 = 3.67 |
  | SJF | 4 6 2 5 1 3 | 2/6 = 0.33 | 20/6 = 3.33 |

  SJF gives the smallest averages. PRIORITY serves pids 5 and 6 at once, but the 1-round job pid 4
  (priority 1) waits 3 rounds.
- With all priorities equal, PRIORITY behaves exactly like ROUND_ROBIN.

## Changes
| file | function | change |
|---|---|---|
| config.h | — | `enum SchedPolicy { ROUND_ROBIN, PRIORITY, SJF }`; `SCHED_POLICY` (default `ROUND_ROBIN`, `#ifndef` so `-DSCHED_POLICY=SJF` works) |
| os.h | — | `create_task(program, data, priority = 0)`; `extern int sched_policy`; `set_sched_policy()`, `parse_priority()` |
| os.cpp | `struct Task` | `priority`, `length` (instructions), `arrival` / `start` / `finish` rounds, `told_waiting` |
| os.cpp | `goes_before`, `insert_by_policy` (new) | the policy comparison; sorted insert used for both queues |
| os.cpp | `policy_name`, `set_sched_policy`, `parse_priority` (new) | policy name, `-s` option, priority syntax `0..9999` |
| os.cpp | `loader` | `insert_by_policy(ready_queue, pid)` instead of `push_back` |
| os.cpp | `admit_waiting` | prints "pid N is waiting" once per task (moved here from `create_task`) |
| os.cpp | `create_task` | stores priority, length and arrival round; inserts into the waiting queue in policy order; no longer loads at once |
| os.cpp | `run_command` | accepts `<program> <data> [priority]`; a non-number priority prints usage |
| os.cpp | `finish_task` | records the finish round and the finish order |
| os.cpp | `print_schedule_report` (new) | per-task table, finish order, averages |
| os.cpp | `scheduler` | prints the policy, counts rounds, records the first-run round, prints the report |
| main.cpp | `main`, `usage` | `-s rr\|priority\|sjf`; optional priority after each program/data pair |
| Makefile | `test` | base regression tests on this build, then `./test.sh` |

## Run
```
cd minicomputer/extensions/e16_priority_scheduling
make
./sim -b -s priority job_long.txt job_data.byte 1  job_mid.txt job_data.byte 2  job_long.txt job_data.byte 3 \
                     job_short.txt job_data.byte 1 job_mid.txt job_data.byte 5  job_short.txt job_data.byte 4
#   same tasks with -s rr or -s sjf; without -s the policy is SCHED_POLICY from config.h
printf 'job_long.txt job_data.byte 1\njob_short.txt job_data.byte 9\nexit\n' | ./sim -s priority   # shell
./test.sh                                  # 31 passed, 0 failed
SIM=$PWD/sim ../../tests/run_tests.sh      # 33 passed, 0 failed (ROUND_ROBIN is still the Lab 4 behaviour)
make clean
```
To choose the policy at compile time instead, set `#define SCHED_POLICY PRIORITY` in `config.h`, then
`make clean && make`.

## Expected output
`-s priority` (the full run):
```
[os] pid 1 created: job_long.txt job_data.byte (50 instructions, priority 1)
[os] pid 2 created: job_mid.txt job_data.byte (30 instructions, priority 2)
[os] pid 3 created: job_long.txt job_data.byte (50 instructions, priority 3)
[os] pid 4 created: job_short.txt job_data.byte (10 instructions, priority 1)
[os] pid 5 created: job_mid.txt job_data.byte (30 instructions, priority 5)
[os] pid 6 created: job_short.txt job_data.byte (10 instructions, priority 4)
[os] scheduling policy PRIORITY
[os] pid 5 (job_mid) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 6 (job_short) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
[os] pid 3 (job_long) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 2 (job_mid) loaded on processor 3: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 1 is waiting for a free processor/frames
[os] pid 4 is waiting for a free processor/frames
[os] pid 6 (job_short) finished on processor 1 after 10 instructions, 0 page fault(s) -> job_data_6.out, job_short_6.freq
[os] pid 1 (job_long) loaded on processor 1: 1 instruction + 1 data page(s), 7 frame(s) left
[os] pid 5 (job_mid) finished on processor 0 after 30 instructions, 0 page fault(s) -> job_data_5.out, job_mid_5.freq
[os] pid 2 (job_mid) finished on processor 3 after 30 instructions, 0 page fault(s) -> job_data_2.out, job_mid_2.freq
[os] pid 4 (job_short) loaded on processor 0: 1 instruction + 1 data page(s), 9 frame(s) left
[os] pid 4 (job_short) finished on processor 0 after 10 instructions, 0 page fault(s) -> job_data_4.out, job_short_4.freq
[os] pid 3 (job_long) finished on processor 2 after 50 instructions, 0 page fault(s) -> job_data_3.out, job_long_3.freq
[os] pid 1 (job_long) finished on processor 1 after 50 instructions, 0 page fault(s) -> job_data_1.out, job_long_1.freq
[os] scheduling report, policy PRIORITY (times in rounds; a round = one slice of 10 instructions for every ready task)
  pid program          prio instr arrival start finish waiting turnaround
    1 job_long            1    50       0     2      6       1          6
    2 job_mid             2    30       0     1      3       0          3
    3 job_long            3    50       0     1      5       0          5
    4 job_short           1    10       0     4      4       3          4
    5 job_mid             5    30       0     1      3       0          3
    6 job_short           4    10       0     1      1       0          1
  finish order: 6 5 2 4 3 1
  average waiting 0.67 rounds, average turnaround 3.67 rounds over 6 task(s)
[os] all tasks finished; print output is in log.txt
```
`log.txt` (each job prints once, just before it ends, so the log shows the finish order):
```
Process id: 6   x1 : 00000006
Process id: 5   x1 : 0000001A
Process id: 2   x1 : 0000001A
Process id: 4   x1 : 00000006
Process id: 3   x1 : 0000002E
Process id: 1   x1 : 0000002E
```
The report for the same tasks with `-s rr` and with `-s sjf`:
```
[os] scheduling report, policy ROUND_ROBIN (times in rounds; a round = one slice of 10 instructions for every ready task)
  pid program          prio instr arrival start finish waiting turnaround
    1 job_long            1    50       0     1      5       0          5
    2 job_mid             2    30       0     1      3       0          3
    3 job_long            3    50       0     1      5       0          5
    4 job_short           1    10       0     1      1       0          1
    5 job_mid             5    30       0     2      4       1          4
    6 job_short           4    10       0     4      4       3          4
  finish order: 4 2 5 6 1 3
  average waiting 0.67 rounds, average turnaround 3.67 rounds over 6 task(s)

[os] scheduling report, policy SJF (times in rounds; a round = one slice of 10 instructions for every ready task)
  pid program          prio instr arrival start finish waiting turnaround
    1 job_long            1    50       0     2      6       1          6
    2 job_mid             2    30       0     1      3       0          3
    3 job_long            3    50       0     2      6       1          6
    4 job_short           1    10       0     1      1       0          1
    5 job_mid             5    30       0     1      3       0          3
    6 job_short           4    10       0     1      1       0          1
  finish order: 4 6 2 5 1 3
  average waiting 0.33 rounds, average turnaround 3.33 rounds over 6 task(s)
```
Shell with priorities (the urgent job, pid 2, gets processor 0 first):
```
[os] scheduling policy PRIORITY
[shell] type '<program> <data.byte> [priority]' to start a task, 'exit' to stop accepting tasks
$ [os] pid 1 created: job_long.txt job_data.byte (50 instructions, priority 1)
$ [os] pid 2 created: job_short.txt job_data.byte (10 instructions, priority 9)
$ [shell] exit: no new tasks; waiting for the running ones to finish
[os] pid 2 (job_short) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
[os] pid 1 (job_long) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
...
  finish order: 2 1
```
`./test.sh` ends with `31 passed, 0 failed`. The base tests (`SIM=$PWD/sim ../../tests/run_tests.sh`)
end with `33 passed, 0 failed`.

## Viva questions
**Q1. With one task per processor, how can a priority change anything?**
Every loaded task runs once per round anyway. Priority decides who gets a processor that becomes
free (the waiting-queue order) and the order inside a round. Finish times change only through that
admission order. It is non-preemptive: a running low-priority task keeps its processor.

**Q2. What would preemptive priority or SRTF need?**
It would need a way to take a processor away: save the victim's registers, PC, flags and PTBR in a
PCB and restore them later (a context switch, see e18). SRTF (shortest remaining time first) would
also need each task's remaining time.

**Q3. What is starvation, and how do you prevent it?**
A low-priority (or long) task can wait forever if better tasks keep arriving. The fix is aging:
every k rounds in `admit_waiting()`, raise the priority of each waiting task (and re-insert it).

**Q4. Why does SJF minimise the average waiting time?**
All jobs here arrive together. Swapping two adjacent jobs of lengths a < b so that the shorter one
runs first changes the total waiting time by a - b < 0. So shortest-first is optimal; in the demo
it gives 0.33 against 0.67.

**Q5. How does a real OS know a job's length?**
It does not know it; it predicts the next CPU burst by exponential averaging,
`tau(n+1) = alpha * t(n) + (1 - alpha) * tau(n)`. Here the length is the program's static
instruction count, which is exact only for straight-line code: a loop is short in the file but long
to run.

**Q6. Why was the loading moved out of `create_task()`?**
Otherwise the first NP command-line tasks are loaded immediately, in typing order, before the
policy can compare them. Queuing them all and loading at the start of the next round lets tasks
that arrive together compete. This also explains why `pid N is waiting` is now printed by
`admit_waiting()`.
