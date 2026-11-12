# Lab 4 — Multiprocessing: NP processors and an OS

The Lab 3 mini-computer (compiler, byte code, vector instructions) now runs several programs at
the same time. There are NP = 4 processors, each with its own registers and memory. A thin
operating system takes programs from the command line and from a shell, loads each one onto a
free processor and shares time between them round robin. A new `print` instruction writes
register values to a log, so the interleaving can be watched.

## What the lab asked
Lab 4 take-home (handout pages 28–40; released 21 Aug 2026, due 28 Aug 2026 9 AM):

- **Everything from Lab 3 still works:** the language, flags, labels and branches, comments, hex
  `.byte` files, and the vector registers and instructions.
- **Multi-processing.** The computer runs several programs at once, and a thin OS layer receives
  them and gives them to processors. The suggested implementation:
  - NP processors, NP fixed at compile time;
  - the register array is two-dimensional, `Register[NP][256]`, and PC and the other registers are arrays of NP;
  - the instruction and data memories are two-dimensional too: `Instruction[NP][256]`, `Data[NP][4096]`;
  - the processor receives a processor id from the OS and works on that processor's registers and memory.
- **Time-division multiplexing.**
  - `process_instructions(proc_id, count)` runs `count` × (fetch, decode, execute) and then sleeps 10 µs, so the execution can be watched.
  - The OS calls it with a time slice of 10 instructions.
- **New instruction `print xR`:**
  - opcode 0x08; dest and operand 1 are 0, and the register goes in operand 2;
  - writes `Process id: … x<R> : <value in hex>` to a log file with `fprintf`;
  - `reset()` opens the log in append mode.
- **OS (`os.c`/`os.h`):** it loads tasks, schedules them and takes new tasks from the user. It keeps
  a list of active processes and the process → processor mapping.
  - **Scheduler:**
    - keeps a ready queue and a waiting queue of tasks, gives each task an ID (PID) and keeps the PID → processor map;
    - runs each ready task for a fixed number of instructions, then the shell;
    - removes a finished task from the queue;
    - puts a new task in the waiting queue when all four processors are busy.
  - **Shell:**
    - reads STDIN without blocking: it takes whatever characters are there into a char array and returns;
    - on Enter it parses the line `prog.txt data.byte`, compiles `prog.txt` into `program.byte`, and passes the program and data to the loader;
    - after `exit` it takes no more input, and only the remaining tasks run.
  - **Loader:**
    - finds a free processor and records the PID → processor mapping;
    - initialises that processor with `program.byte` and `data.byte`;
    - puts the task in the ready queue.
- **main:** compile, initialise, reset, the run loop and finalize all move into the OS.
- **Files:** `main`, `compiler`, `processor`, `memory`, `os` (source + header) and a `Makefile`.
- **Example programs:** the sum of an array with `print x1` in its loop, plus the Lab 3 array add and FIR filter.

## What is implemented
| Feature | File / function |
|---|---|
| `NP 4`, `TIME_SLICE 10`, `SLEEP_US 10`, `LOG_FILE`, `MAX_TASKS`, sizes, `OP_PRINT = 0x08` | `config.h` |
| `print xR` → `08 00 00 R` (plus everything from Lab 3) | `compiler.cpp` `encode_statement()`, `disassemble()` |
| A `.byte` program can be given instead of source | `compiler.cpp` `load_bytecode()` |
| Per-processor state: `Register[NP][256]`, `VRegister[NP][32][8]`, `PC[NP]`, flags `[NP]`, `end_of_simulation[NP]`, `proc_id`, `fd_log` | `processor.h`, `processor.cpp` |
| `reset(p)`: clears processor p and opens `log.txt` in append mode | `processor.cpp` `reset()` |
| `fetch()` / `decode()` / `execute()` on processor `proc_id` and its memory | `processor.cpp` |
| `print`: `fprintf(fd_log, "Process id: %d   x%d : %08X\n", ...)` | `processor.cpp` `execute()` case `OP_PRINT` |
| `process_instructions(p, count)`: up to `count` instructions, then `usleep(10)` | `processor.cpp` |
| `Instruction[NP][256]`, `Data[NP][4096]`, `data_size[NP]`, `.byte` reader/writer | `memory.cpp` |
| Process table (`struct Task`: pid, state, processor, byte code, data, steps), ready and waiting queues | `os.cpp` |
| Task creation: compile (writes `program.byte`) or read the `.byte` program, read the data, queue the task | `os.cpp` `create_task()` |
| Loader: free processor, PID → processor map, `reset(p)`, `initialize(p)`, ready queue | `os.cpp` `loader()`, `admit_waiting()`, `map_pid_proc_id()` |
| `initialize()`: copy the program and data into `Instruction[p]` / `Data[p]` (moved into the OS) | `os.cpp` `initialize()` |
| `finalize()`: write `Data[p]` to `<data>_<pid>.out` (moved into the OS) | `os.cpp` `finalize()`, `finish_task()` |
| Round-robin scheduler: each round, every ready task gets `TIME_SLICE` instructions, then the shell | `os.cpp` `scheduler()` |
| Non-blocking shell: `select()` with a zero timeout + `read()`, a line buffer, `exit`, end of input | `os.cpp` `shell()`, `run_command()` |
| Command line: tasks as pairs, `-b` batch (no shell), `-c` compile listing | `main.cpp` |
| Build | `Makefile` (`make`, `make test`, `make clean`) |
| Counters with `print`, the handout's sum program, array add and FIR | `examples/` |
| Checks | `test.sh` (35 checks) |

## Byte code
Every instruction is 4 bytes: `<opcode> <dest> <operand 1> <operand 2>`; `c` is a constant 0..255.
The compiler ends every program with `00 00 00 00`.

| Statement | Opcode | Encoding |
|---|---|---|
| `xD = xA + xB` / `-` / `*` / `/` | 01 02 03 04 | `op D A B` |
| `xD = xA + c` / `-` / `*` / `/` | 09 0A 0B 0C | `op D A c` |
| `xD = [xA]` | 05 | `05 D 0 A` |
| `[xA] = xS` | 06 | `06 A 0 S` |
| `xD = xA` | 07 | `07 D 0 A` |
| **`print xR`** (Lab 4) | **08** | **`08 0 0 R`** |
| `xD = [c]`, `Read xD c` | 0D | `0D D 0 c` |
| `[c] = xS`, `Write xS c` | 0E | `0E c 0 S` |
| `xD = c` | 0F | `0F D 0 c` |
| `B{cond} .label` | 10 + cond | `1k 0 0 offset` (signed, in instructions) |
| `vD = vA + vB` / `-` / `*` | 21 22 23 | `op D A B` |
| `vD = [xA]` | 25 | `25 D 0 A` |
| `[xA] = vS` | 26 | `26 A 0 S` |
| `vD = vA + c` / `-` / `*` | 29 2A 2B | `op D A c` |
| `vD = [c]` | 2C | `2C D 0 c` |
| `[c] = vS` | 2E | `2E c 0 S` |
| `vD = vA + xB` / `-` / `*` | 31 32 33 | `op D A B` (B is an x register) |
| `vD = vA` (copy) | 29 | `29 D A 00`, i.e. `vD = vA + 0` |
| end of program | 00 | `00 00 00 00` |

Branch conditions, ARM order (`B` alone = `BAL`; `BHS` = `BCS`, `BLO` = `BCC`):
0 EQ (Z), 1 NE (!Z), 2 CS (C), 3 CC (!C), 4 MI (N), 5 PL (!N), 6 VS (V), 7 VC (!V),
8 HI (C && !Z), 9 LS (!C || Z), 10 GE (N == V), 11 LT (N != V), 12 GT (!Z && N == V), 13 LE (Z || N != V), 14 AL.

Only scalar add and subtract change the flags. Z: the result is 0. N: bit 31 of the result.
C: for add, the unsigned result is smaller than an input; for subtract, operand 1 > operand 2 (unsigned).
V: for add, the operands have the same sign and the result has the other; for subtract, the
operand signs differ and the result has operand 2's sign.

## Build and run
```bash
make                                    # g++ -std=c++17 -O2 -Wall -Wextra: no warnings
make test                               # 35 checks (about 5 s), prints "35 passed, 0 failed"

./sim -b examples/counter.txt examples/counter_data.byte examples/fir.txt examples/fir_data.byte
                                        # batch: run these tasks, no shell, stop when all have finished
./sim examples/counter.txt examples/counter_data.byte
                                        # the same, then the shell: type "<program> <data>" lines,
                                        # then "exit" (or Ctrl-D) once you want no more tasks
./sim                                   # shell only: start every task by typing it
./sim -c examples/sum_array.txt         # compile only: write program.byte and print the listing
make clean                              # remove sim, *.o, program.byte, *.out, log.txt
```
- **Programs:** a program is either source (compiled first; the byte code is also written to
  `program.byte`) or an already compiled `.byte` file.
- **Output of each task:** `<data>_<pid>.out` in the current directory, holding the task's final data memory.
- **`print` output:** goes to `log.txt`. It is appended to, so delete it for a fresh log.
- **Exit code:** 0 when every task ran to its end.

## Examples
| Program | Data | Shows |
|---|---|---|
| `counter.txt` | `counter_data.byte` | Counts to 10, printing each value; stores 10 at 0 |
| `counter_by100.txt` | `counter_data.byte` | Counts 100, 200, …, 1000 (easy to tell apart in the log); stores 1000 at 0 |
| `sum_array.txt` | `sum_array_data.byte` | The handout's Lab 4 example: sum of 1..9 with `print x1` in the loop; 45 at 36 |
| `array_add.txt` | `array_add_data.byte` | Lab 3 array add with vectors: C = 101, 103, …, 131 from 132 |
| `fir.txt` | `fir_data.byte`, `fir64_data.byte` | Lab 3 FIR with vectors (N = 24 and N = 64); output from 0x100 |

The examples below were run from this folder (real output).

**`print` in the byte code.** The handout's Lab 4 sum program has `print x1` at instruction 10:
```text
$ ./sim -c examples/sum_array.txt
examples/sum_array.txt -> program.byte (14 instructions)
  0: 0F 01 00 00   x1 = 0             | x1 = 0       % index
  1: 0F 02 00 00   x2 = 0             | x2 = 0        % sum
  2: 0F 03 00 00   x3 = 0             | x3 = 0        % array address
  3: 05 04 00 03   x4 = [x3]          | x4 =  [x3]    %  read the size of array
  4: 02 0F 01 04   x15 = x1 - x4      | x15 = x1 - x4   % compare
  5: 10 00 00 07   BEQ +7             | BEQ .exit
  6: 09 03 03 04   x3 = x3 + 4        | x3 = x3 + 4    % increase the address
  7: 05 06 00 03   x6 = [x3]          | x6 =  [x3]       % load the array data
  8: 01 02 02 06   x2 = x2 + x6       | x2 = x2 + x6  % update sum
  9: 09 01 01 01   x1 = x1 + 1        | x1 = x1 + 1    % increment index
 10: 08 00 00 01   print x1           | print x1
 11: 1E 00 00 F9   BAL -7             | BAL .loopback
 12: 06 03 00 02   [x3] = x2          | [x3] = x2
 13: 00 00 00 00   halt               | (end of program)
```

**Five tasks on four processors.** Pid 5 waits until pid 1 finishes, then gets processor 0:
```text
$ E=examples
$ ./sim -b $E/counter.txt $E/counter_data.byte $E/counter_by100.txt $E/counter_data.byte $E/counter.txt $E/counter_data.byte \
           $E/counter_by100.txt $E/counter_data.byte $E/counter.txt $E/counter_data.byte
[os] pid 1 created: examples/counter.txt examples/counter_data.byte (7 instructions)
[os] pid 1 (counter) loaded on processor 0
[os] pid 2 created: examples/counter_by100.txt examples/counter_data.byte (9 instructions)
[os] pid 2 (counter_by100) loaded on processor 1
[os] pid 3 created: examples/counter.txt examples/counter_data.byte (7 instructions)
[os] pid 3 (counter) loaded on processor 2
[os] pid 4 created: examples/counter_by100.txt examples/counter_data.byte (9 instructions)
[os] pid 4 (counter_by100) loaded on processor 3
[os] pid 5 created: examples/counter.txt examples/counter_data.byte (7 instructions)
[os] pid 5 is waiting: all 4 processors are busy
[os] pid 1 (counter) finished on processor 0 after 43 instructions -> counter_data_1.out
[os] pid 3 (counter) finished on processor 2 after 43 instructions -> counter_data_3.out
[os] pid 5 (counter) loaded on processor 0
[os] pid 2 (counter_by100) finished on processor 1 after 54 instructions -> counter_data_2.out
[os] pid 4 (counter_by100) finished on processor 3 after 54 instructions -> counter_data_4.out
[os] pid 5 (counter) finished on processor 0 after 43 instructions -> counter_data_5.out
[os] all tasks finished; print output is in log.txt
$ head -8 log.txt
Process id: 1   x1 : 00000001
Process id: 1   x1 : 00000002
Process id: 2   x1 : 00000064
Process id: 2   x1 : 000000C8
Process id: 3   x1 : 00000001
Process id: 3   x1 : 00000002
Process id: 4   x1 : 00000064
Process id: 4   x1 : 000000C8
$ awk '{print $3}' log.txt | paste -sd, -       # pid of each of the 50 log lines
1,1,2,2,3,3,4,4,1,1,1,2,2,3,3,3,4,4,1,1,2,2,3,3,4,4,1,1,1,2,2,3,3,3,4,4,2,2,4,4,5,5,5,5,5,5,5,5,5,5
```
Each 10-instruction slice of a counter prints 2 or 3 times, so the log shows the round robin:
1, 2, 3, 4, then 1, 2, 3, 4 again. Pid 5 prints only after a processor is free.

**Lab 3 programs side by side.** Each task has its own `Data[p]`, so they never interfere:
```text
$ ./sim -b $E/array_add.txt $E/array_add_data.byte $E/fir.txt $E/fir_data.byte $E/sum_array.txt $E/sum_array_data.byte \
           $E/fir.txt $E/fir64_data.byte
[os] pid 1 created: examples/array_add.txt examples/array_add_data.byte (18 instructions)
[os] pid 1 (array_add) loaded on processor 0
[os] pid 2 created: examples/fir.txt examples/fir_data.byte (52 instructions)
[os] pid 2 (fir) loaded on processor 1
[os] pid 3 created: examples/sum_array.txt examples/sum_array_data.byte (14 instructions)
[os] pid 3 (sum_array) loaded on processor 2
[os] pid 4 created: examples/fir.txt examples/fir64_data.byte (52 instructions)
[os] pid 4 (fir) loaded on processor 3
[os] pid 1 (array_add) finished on processor 0 after 27 instructions -> array_add_data_1.out
[os] pid 3 (sum_array) finished on processor 2 after 80 instructions -> sum_array_data_3.out
[os] pid 2 (fir) finished on processor 1 after 88 instructions -> fir_data_2.out
[os] pid 4 (fir) finished on processor 3 after 268 instructions -> fir64_data_4.out
[os] all tasks finished; print output is in log.txt
```

**The shell.** The typed lines come from a pipe here, so they are not echoed after the `$` prompt.
The FIR task finishes while the shell is still waiting for input. Two tasks are then typed, plus
one bad line, and `exit` follows a second later:
```text
$ (sleep 1; echo "examples/counter.txt examples/counter_data.byte"; echo "examples/counter_by100.txt examples/counter_data.byte"; \
   echo "hello"; sleep 1; echo exit) | ./sim examples/fir.txt examples/fir64_data.byte
[os] pid 1 created: examples/fir.txt examples/fir64_data.byte (52 instructions)
[os] pid 1 (fir) loaded on processor 0
[shell] type '<program> <data.byte>' to start a task, 'exit' to stop accepting tasks
$ [os] pid 1 (fir) finished on processor 0 after 268 instructions -> fir64_data_1.out
[os] pid 2 created: examples/counter.txt examples/counter_data.byte (7 instructions)
[os] pid 2 (counter) loaded on processor 0
$ [os] pid 3 created: examples/counter_by100.txt examples/counter_data.byte (9 instructions)
[os] pid 3 (counter_by100) loaded on processor 1
$ [shell] usage: <program.txt|program.byte> <data.byte>   or   exit
$ [os] pid 2 (counter) finished on processor 0 after 43 instructions -> counter_data_2.out
[os] pid 3 (counter_by100) finished on processor 1 after 54 instructions -> counter_data_3.out
[shell] exit: no new tasks; waiting for the running ones to finish
[os] all tasks finished; print output is in log.txt
```

`make test` builds with `make -B`, checks that there are no compiler warnings, and runs 35 checks in
a temporary directory:
- `print`: its byte code and the log-line format;
- the handout's Lab 4 sum program and its byte code;
- five counters: the PID → processor map, the waiting queue, separate data memories, and the
  whole 50-line log order (round robin);
- `log.txt` is appended to;
- four Lab 3 programs at once, with the array add and FIR results compared with a reference computed in awk;
- a precompiled `.byte` task;
- the shell: tasks run while it waits for input, a line runs only on Enter, `exit`, end of input, a bad line;
- an error in one task leaves the others running.

## Decisions
| The handout says / leaves open | What this code does |
|---|---|
| "Instruction and data memory are also two dimension" | Each processor has its own `Instruction[p][256]` and `Data[p][4096]`; there is no paging (that is Lab 5). Loading a task copies its program and data there |
| The scheduler pseudo code calls `process_instructions(pid, 10)` | It looks up `proc_id = map_pid_proc_id(pid)` and runs that processor: `process_instructions(p, TIME_SLICE)` |
| "Progam.txt is compiled into program.byte… sends the file names to the loader" | The task is compiled when it is created, and `program.byte` is written as before. The process table keeps the byte code and the data image, so a task that waits can still be loaded after another program has overwritten `program.byte` |
| `finalize()` "writes data.byte" | Each task writes `<data>_<pid>.out`, so tasks sharing a data file never overwrite each other or their input |
| Print format `Process id:   x<variable> : <value in hex>` (the pid value is missing in the template) | `Process id: <pid>   x<R> : <8 hex digits>`, e.g. `Process id: 1   x1 : 0000000A`; a negative value shows as 32-bit hex (`FFFFFFFB` = −5) |
| The log file is opened in `reset()`, in append mode | `reset(p)` opens it on first use; it stays open for every task and is closed when the scheduler ends. Runs add to it, so delete it for a fresh log |
| Order in the waiting queue | First come, first served. A waiting task is loaded at the start of the next round, once a processor is free |
| When the OS stops | When the shell is closed (`exit` or end of input) and no task is ready or waiting. `-b` starts with the shell closed. While nothing is ready, the scheduler polls the shell every 10 ms instead of spinning |
| The handout's Lab 4 sum example shows the Lab 2 byte code | With `print x1` the program has one more instruction: `print x1` = `08 00 00 01`, `BEQ` = `10 00 00 07`, `BAL` = `1E 00 00 F9` (checked by `make test`) |
| A failing task | A run-time error (division by zero, address outside `Data[p]`, …) stops only that task, with `[pid N, processor P] error at instruction I: …`; the others keep running and the exit code is 1 |
| Lab 3 decisions | Still apply: `xD = [c]` is 0x0D (0x0C is divide-by-constant), `vD = vA op xB` is 0x31–0x33, ARM condition order, 32-bit little-endian words, vectors leave the flags alone, and the FIR computes its blocks from last to first because the N = 64 output overlaps the input |

## Viva questions
1. **How does one program simulate 4 processors?**
   Every piece of processor state is an array indexed by processor: `Register[NP][256]`, `PC[NP]`,
   the flags, `end_of_simulation[NP]` and the memories. `process_instructions(p, n)` sets
   `proc_id = p`, and fetch/decode/execute use index `p`. Each processor keeps its own state, so
   switching tasks needs no save or restore.
2. **What does the OS keep for each task?**
   A process-table entry (`struct Task`): pid, state (waiting/ready/done), processor, program byte code,
   initial data, and instruction count. `proc_owner[NP]` and `map_pid_proc_id()` give the
   PID ↔ processor mapping.
3. **Explain the round robin.**
   Each round takes every task in the ready queue once, runs it for `TIME_SLICE` = 10 instructions,
   and puts it back at the end of the queue unless it finished. Then `shell()` runs. In the log, each
   counter prints 2 or 3 times per slice, in pid order 1, 2, 3, 4.
4. **What happens to a fifth task?**
   With all 4 processors busy, the loader cannot place it, so it goes to the waiting queue. When pid 1
   finishes, its processor is freed. At the start of the next round, the loader puts pid 5 on processor 0.
5. **How is the shell non-blocking?**
   `select()` with a zero timeout asks whether stdin has data; only then does `read()` run, so it never
   waits. Characters collect in a buffer, and a command runs only when its `\n` arrives. A terminal
   delivers a whole line at a time anyway (canonical mode).
6. **Why `usleep(10)` after each slice?**
   The handout asks for it so the interleaved execution can be watched in real time. It also hands the
   host CPU back between slices.
7. **Why does each task write its own `.out` file?**
   Several tasks run at once, and two may even use the same `data.byte`. If each one wrote back to
   `data.byte`, they would overwrite each other's results.
8. **What changes in Lab 5?**
   One shared physical memory replaces the per-processor arrays. Each task gets pages mapped to free
   frames through a page table, so a task's memory need not be contiguous or a fixed slot per processor.
