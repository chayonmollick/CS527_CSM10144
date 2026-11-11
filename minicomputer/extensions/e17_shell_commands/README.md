# e17 — Shell commands: ps, kill, mem, help

## Question (as it could appear in the test)
Extend the shell of your Lab 4-6 simulator with these commands:
- `ps`: list every task with its pid, program, state, processor, instructions executed so far and pages (frames) held.
- `kill <pid>`: stop a running or waiting task. Free its processor and its frames, and still write its output, marked as killed.
- `mem`: show how many physical frames are free, out of NUM_PHYSICAL_PAGES-1.
- `help`: list the commands. Any unknown command must print the help as well.

Demonstrate it by piping a command sequence into the simulator: start a long-running task, `ps`, kill it, `ps` again, then `exit`.

## Solution idea
- **Where the commands run.** All commands are handled in `run_command()`, which already splits a line into words. The shell runs once per scheduler round, between rounds, so no task is ever in the middle of its time slice when a command executes.
- **`ps`** walks the process table `tasks`. A loaded task's live state is still in its processor's arrays:
  - instructions so far = `steps[p]`;
  - pages held = the number of non-zero entries in its page table in frame 0 (`memory[PTBR[p] + i]`).
  - Finished tasks show the values saved in `Task`.
- **`kill` on a READY (loaded) task:**
  1. Remove the pid from the ready queue and set `t.killed`.
  2. Call the normal end-of-task path `finish_task()`. Its `finalize()` writes `<data>_<pid>.out` and `<prog>_<pid>.freq` as they are at that moment, frees every frame listed in the page table, clears the table and sets `PTBR[p] = -1`. `proc_owner[p] = 0` frees the processor.
  3. The `[os]` line says **KILLED** and `ps` shows state KILLED.
  4. At the start of the next round, `admit_waiting()` can load a waiting task on the freed processor and frames.
- **`kill` on a WAITING task.** It has no processor and no frames, so it is only removed from the waiting queue. Its output file is its initial data, because it never ran (no `.freq`).
- **`mem`** prints `free_page_count()` and a frame map. The map is built by reading the page tables of the loaded tasks: frame -> pid, with frame 0 holding the page tables (PT).
- **`sleep <n>`** (extra). The shell reads stdin without blocking, so a piped script is read in one go before round 1. `sleep 3` tells the shell to run no further command for 3 rounds, so "ps after 3 rounds" is deterministic and needs no wall-clock timing.
- **End of input.** The base shell closes as soon as stdin reaches its end. That is right in the base, because every complete line has already run by then. With `sleep`, lines can still be queued at that moment, so the shell now remembers end of input and closes only after every queued line has run, including a last line without a newline.

## Changes
| file | function | change |
|---|---|---|
| os.cpp | `enum TaskState`, `struct Task` | new state `KILLED`, new flag `killed` |
| os.cpp | globals | `input_ended` (stdin hit EOF), `shell_sleep` (rounds left in `sleep n`) |
| os.cpp | `finalize()` | message says `KILLED` instead of `finished` for a killed task |
| os.cpp | `state_name()`, `frames_held()` | new: state text for ps (FAILED = DONE with an error); count of mapped page-table entries |
| os.cpp | `ps()`, `mem()`, `kill_task()`, `help()`, `to_number()` | new: the commands (kill handles READY and WAITING tasks) |
| os.cpp | `run_command()` | dispatches `ps`, `mem`, `help`, `kill <pid>`, `sleep <n>`; unknown command -> message + help |
| os.cpp | `shell()` | `sleep n` pauses commands for n rounds; end of input closes the shell only after all queued lines |
| os.cpp | `finish_task()` | state becomes `KILLED` or `DONE` |
| os.cpp | `scheduler()` | start-up hint mentions `help` |
| Makefile | header, `test` | `make test` = base tests on this build, then `./test.sh` |
| loop.txt, loop_data.byte | — | example: endless counter, stores the count at [0] each step |
| test.sh | — | 25 checks: 2 piped sessions |

`grep -n "EXT e17" *.cpp *.h Makefile` lists every change; `CHANGES.diff` has the full diff against `simulator/`.

## Run
```
cd minicomputer/extensions/e17_shell_commands
make
./sim                                   # interactive: type the commands at the $ prompt
printf 'loop.txt loop_data.byte\nloop.txt loop_data.byte\n../../examples/counter.txt ../../examples/counter_data.byte\nsleep 3\nps\nmem\nkill 1\nps\nmem\nkill 1\nfoo\nkill 2\nexit\n' | ./sim
./test.sh                               # 25 passed, 0 failed
SIM=$PWD/sim ../../tests/run_tests.sh   # base regression tests: 33 passed, 0 failed
make clean
```

## Expected output
The piped session above. stdin is not echoed, so each `$ ` prompt is followed directly by the output of the next command.
```
[shell] type '<program> <data.byte>' to start a task, 'help' for ps/kill/mem, 'exit' to stop accepting tasks
$ [os] pid 1 created: loop.txt loop_data.byte (5 instructions)
[os] pid 1 (loop) loaded on processor 0: 1 instruction + 1 data page(s), 13 frame(s) left
$ [os] pid 2 created: loop.txt loop_data.byte (5 instructions)
[os] pid 2 (loop) loaded on processor 1: 1 instruction + 1 data page(s), 11 frame(s) left
$ [os] pid 3 created: ../../examples/counter.txt ../../examples/counter_data.byte (7 instructions)
[os] pid 3 (counter) loaded on processor 2: 1 instruction + 1 data page(s), 9 frame(s) left
$ [shell] sleep: next command after 3 round(s)
$   PID  PROGRAM         STATE    PROC   INSTR  PAGES
    1  loop            READY       0      30      2
    2  loop            READY       1      30      2
    3  counter         READY       2      30      2
$ [mem] 9 of 15 frames free (frame 0 holds the page tables)
[mem] frame   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
[mem] owner  PT   1   1   2   2   3   3   .   .   .   .   .   .   .   .   .
$ [os] pid 1 (loop) KILLED on processor 0 after 30 instructions, 0 page fault(s) -> loop_data_1.out, loop_1.freq
$   PID  PROGRAM         STATE    PROC   INSTR  PAGES
    1  loop            KILLED      -      30      0
    2  loop            READY       1      30      2
    3  counter         READY       2      30      2
$ [mem] 11 of 15 frames free (frame 0 holds the page tables)
[mem] frame   0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15
[mem] owner  PT   .   .   2   2   3   3   .   .   .   .   .   .   .   .   .
$ [shell] kill: pid 1 is not running or waiting (it is KILLED)
$ [shell] unknown command 'foo'
[shell] usage: <program.txt|program.byte> <data.byte>   start a task
        ps            list tasks: pid, program, state, processor, instructions, pages
        kill <pid>    stop a running or waiting task (its output is still written)
        mem           free frames and the owner of every frame
        sleep <n>     read the next command after n scheduler rounds
        help          this text
        exit          accept no new tasks; finish the running ones
$ [os] pid 2 (loop) KILLED on processor 1 after 30 instructions, 0 page fault(s) -> loop_data_2.out, loop_2.freq
$ [shell] exit: no new tasks; waiting for the running ones to finish
[os] pid 3 (counter) finished on processor 2 after 43 instructions, 0 page fault(s) -> counter_data_3.out, counter_3.freq
[os] all tasks finished; print output is in log.txt
```
Output files: `loop_data_1.out` = `0A 00 00 00` and `loop_data_2.out` = `0A 00 00 00`. Each killed task had executed 30 instructions: 1 for `x1 = 0`, 9 full iterations of 3 instructions, then `x1 = 10` and `[0] = x1`. `counter_data_3.out` = `0A 00 00 00`, a normal finish.

`./test.sh` (shortened):
```
Scenario 1: 3 endless loops + 2 counters on 4 processors; ps, mem, kill
  ok    ps: pid 2 running, then killed
  ok    ps: pid 5 waiting, then killed
  ok    ps: pid 6 reuses processor 1
  ok    mem: frames 3,4 freed by kill 2, then given to pid 6
  ok    killed pid 2 wrote its .freq (counts at the kill)
  ...
Scenario 2: end of input while sleeping; last line has no newline; a failed task in ps
  ok    last line 'kill 1' ran after the sleep: [0] = 6 after 20 instructions
  ...
25 passed, 0 failed
```

## Viva questions
1. **Why is it safe for the shell to kill a task without stopping the processor in the middle of an instruction?**
   The shell runs only between scheduler rounds. Every task has completed its time slice, so its processor state is consistent. Killing it just means taking it off the ready queue and running the normal `finalize()`.
2. **What does `kill` free, and how does the OS know which frames to free?**
   It frees the processor (`proc_owner[p] = 0`, `PTBR[p] = -1`) and every frame named in the task's page table in frame 0 (`memory[PTBR[p] + i]`, each set free in `freePages`). The next round's `admit_waiting()` can then load a waiting task.
3. **How does `ps` know how many instructions a running task has executed, and how many pages it holds?**
   Instructions come from `steps[p]` of its processor, which counts every executed instruction; the count is copied into `Task` only when the task ends. Pages are the number of non-zero entries in its page table; demand-mapped data pages are included.
4. **Why does a killed WAITING task have an output file equal to its input data?**
   It was never loaded. It has no frames and no processor, so its memory is still its initial data image.
5. **How is this different from Unix `kill`?**
   Unix `kill` sends a signal: SIGTERM can be caught, SIGKILL cannot. The killed process then stays a zombie until its parent calls `wait()`. Here the OS stops the task at once and reclaims everything, writing its data much like a core dump.
6. **Why add `sleep <n>`?**
   The shell reads stdin without blocking, once per round, so a piped script is read entirely before round 1. `sleep n` lets n rounds run before the next command. That makes "ps after some work" reproducible with no wall-clock timing.
