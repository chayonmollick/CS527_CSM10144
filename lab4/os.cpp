/* Operating System (handout "Operating System (OS)", Lab 4): process table, Scheduler,
 * Shell and Loader, each in its own section below.
 *
 * - Up to NP tasks run at once, one per processor; each processor has its own memory
 *   (Instruction[p], Data[p]), so loading a task is just copying its program and data there.
 * - Scheduler: every round gives each task in the ready queue TIME_SLICE instructions
 *   (round robin), then runs the shell. A finished task is removed from the queue.
 * - A task that finds every processor busy waits in the waiting queue until one is free.
 * - Shell: reads stdin without blocking; "<program> <data>" starts a task, "exit" stops input.
 */
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <deque>
#include <sstream>
#include <vector>
#include <sys/select.h>
#include <unistd.h>
#include "compiler.h"
#include "memory.h"
#include "os.h"
#include "processor.h"

enum TaskState { WAITING, READY, DONE };

struct Task {
    int pid = 0;
    TaskState state = WAITING;
    int proc = -1;                          /* processor while loaded, else -1 */
    std::string program_path, data_path;
    Program prog;                           /* byte code (compiled program.byte) */
    std::vector<unsigned char> data;        /* initial data image (data.byte) */
    long steps = 0;
    bool failed = false;
};

static std::vector<Task> tasks;             /* process table: tasks[pid - 1] */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* pid running on processor p, 0 = free */
static bool shell_open = true;
static std::string shell_buffer;            /* characters typed but not yet ended by Enter */

/* ---------------- initialize() / finalize(): moved into the OS (handout "Main file") ---------------- */

/* initialize(): copy the task's program and data into the memory of processor p. */
static void initialize(int p, const Task &t) {
    std::memset(Instruction[p], 0, INSTR_SIZE);         /* unused bytes read as 00 00 00 00 = end of program */
    std::memset(Data[p], 0, DATA_SIZE);
    std::copy(t.prog.code.begin(), t.prog.code.end(), Instruction[p]);
    std::copy(t.data.begin(), t.data.end(), Data[p]);
    data_size[p] = (int)t.data.size();
}

/* finalize(): write the data memory of processor p to <data>_<pid>.out. */
static void finalize(int p, Task &t) {
    size_t size = ((size_t)data_size[p] + 3) / 4 * 4;  /* whole 32-bit words */
    std::string out_name = file_stem(t.data_path) + "_" + std::to_string(t.pid) + ".out";
    bool written = write_byte_file(out_name, std::vector<unsigned char>(Data[p], Data[p] + size));
    t.steps = steps[p];
    t.failed = failed[p];
    std::printf("[os] pid %d (%s) %s on processor %d after %ld instructions -> %s\n",
                t.pid, t.prog.name.c_str(), t.failed ? "STOPPED WITH AN ERROR" : "finished", p,
                t.steps, written ? out_name.c_str() : "(could not write the output file)");
}

/* ---------------- Loader, and creating a task ---------------- */

/* loader(): give the task a free processor (PID -> processor map), reset that processor,
 * initialize its memory with the task's program and data, and put the task in the ready queue. */
static bool loader(int pid) {
    int p = -1;
    for (int i = 0; i < NP; i++)
        if (!proc_owner[i]) { p = i; break; }
    if (p < 0) return false;                /* every processor is busy: the task keeps waiting */

    Task &t = tasks[pid - 1];
    proc_owner[p] = pid;
    t.proc = p;
    reset(p);
    current_pid[p] = pid;
    initialize(p, t);
    t.state = READY;
    ready_queue.push_back(pid);
    std::printf("[os] pid %d (%s) loaded on processor %d\n", pid, t.prog.name.c_str(), p);
    return true;
}

/* Start waiting tasks, oldest first, while processors are free. */
static void admit_waiting() {
    while (!waiting_queue.empty() && loader(waiting_queue.front()))
        waiting_queue.pop_front();
}

int map_pid_proc_id(int pid) {
    return (pid >= 1 && pid <= (int)tasks.size()) ? tasks[pid - 1].proc : -1;
}

int failed_task_count() {
    int n = 0;
    for (const Task &t : tasks) n += t.failed;
    return n;
}

/* A new task: compile prog.txt (writing program.byte) or read a .byte program, read data.byte,
 * enter it in the process table and the waiting queue, and load it if a processor is free. */
int create_task(const std::string &program, const std::string &data) {
    if (tasks.size() >= MAX_TASKS) { std::printf("[os] process table full (%d tasks)\n", MAX_TASKS); return -1; }
    Task t;
    t.pid = (int)tasks.size() + 1;
    t.program_path = program;
    t.data_path = data;
    std::string err;
    bool is_bytecode = program.size() > 5 && program.compare(program.size() - 5, 5, ".byte") == 0;
    bool ok = is_bytecode ? load_bytecode(program, t.prog, err) : compile(program, t.prog, err);
    if (ok && !is_bytecode) write_program_byte("program.byte", t.prog);     /* the compiled code */
    if (ok) ok = read_byte_file(data, t.data, err);
    if (ok && t.data.size() > (size_t)DATA_SIZE) {
        err = data + ": " + std::to_string(t.data.size()) + " bytes do not fit in Data[" + std::to_string(DATA_SIZE) + "]";
        ok = false;
    }
    if (!ok) { std::printf("[os] cannot start task: %s\n", err.c_str()); return -1; }

    tasks.push_back(t);
    waiting_queue.push_back(t.pid);
    std::printf("[os] pid %d created: %s %s (%zu instructions)\n", t.pid, program.c_str(), data.c_str(), t.prog.code.size() / 4);
    admit_waiting();
    if (tasks.back().state == WAITING)
        std::printf("[os] pid %d is waiting: all %d processors are busy\n", t.pid, NP);
    return t.pid;
}

/* ---------------- Shell ---------------- */

static void prompt() {
    if (shell_open) { std::printf("$ "); std::fflush(stdout); }
}

/* One complete line typed at the shell. */
static void run_command(const std::string &line) {
    std::istringstream in(line);
    std::vector<std::string> words;
    std::string w;
    while (in >> w) words.push_back(w);
    if (words.empty()) { prompt(); return; }
    if (words[0] == "exit") {
        shell_open = false;
        std::printf("[shell] exit: no new tasks; waiting for the running ones to finish\n");
        return;
    }
    if (words.size() == 2) create_task(words[0], words[1]);
    else std::printf("[shell] usage: <program.txt|program.byte> <data.byte>   or   exit\n");
    prompt();
}

/* shell(): non-blocking - takes only the characters already waiting on stdin (select with a
 * zero timeout, then read), keeps them in a buffer, and runs each line once Enter ends it. */
void shell() {
    if (!shell_open) return;
    bool end_of_input = false;
    for (;;) {                                         /* take whatever is already typed */
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        timeval no_wait = {0, 0};
        if (select(1, &fds, nullptr, nullptr, &no_wait) <= 0) break;
        char buf[256];
        ssize_t n = read(0, buf, sizeof buf);
        if (n <= 0) { end_of_input = true; break; }
        shell_buffer.append(buf, (size_t)n);
    }
    size_t nl;                                         /* run every complete line */
    while (shell_open && (nl = shell_buffer.find('\n')) != std::string::npos) {
        std::string line = shell_buffer.substr(0, nl);
        shell_buffer.erase(0, nl + 1);
        run_command(line);
    }
    if (end_of_input && shell_open) {                  /* end of input acts like "exit" */
        if (!shell_buffer.empty()) { run_command(shell_buffer); shell_buffer.clear(); }
        if (shell_open) std::printf("\n[shell] end of input: no new tasks\n");
        shell_open = false;
    }
}

/* ---------------- Scheduler ---------------- */

void os_init(bool batch) {
    for (int p = 0; p < NP; p++) proc_owner[p] = 0;
    shell_open = !batch;
}

/* The task on its processor has ended: write its data, free the processor. */
static void finish_task(int pid) {
    Task &t = tasks[pid - 1];
    int p = t.proc;
    finalize(p, t);
    t.state = DONE;
    t.proc = -1;
    proc_owner[p] = 0;
}

/* scheduler(): round robin. Each round, every ready task runs TIME_SLICE instructions on
 * its processor; then the shell gets its turn. Ends when the shell is closed and no task is left. */
void scheduler() {
    if (shell_open) {
        std::printf("[shell] type '<program> <data.byte>' to start a task, 'exit' to stop accepting tasks\n");
        prompt();
    }
    for (;;) {
        admit_waiting();                               /* a processor may have been freed */
        size_t n = ready_queue.size();
        for (size_t i = 0; i < n; i++) {               /* one round: every ready task gets a slice */
            int pid = ready_queue.front();
            ready_queue.pop_front();
            int p = map_pid_proc_id(pid);
            process_instructions(p, TIME_SLICE);
            if (end_of_simulation[p]) finish_task(pid);   /* finished: leaves the queue */
            else ready_queue.push_back(pid);
        }
        shell();
        if (!shell_open && ready_queue.empty() && waiting_queue.empty()) break;
        if (ready_queue.empty()) usleep(10000);        /* nothing to run: wait for typing */
    }
    std::printf("[os] all tasks finished; print output is in %s\n", LOG_FILE);
    if (fd_log) { std::fclose(fd_log); fd_log = nullptr; }
}
