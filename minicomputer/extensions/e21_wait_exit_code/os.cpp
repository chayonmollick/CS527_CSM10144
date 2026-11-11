/* Operating system: tasks, scheduling, shell, loader and memory management.
 *
 * Lab 4: up to NP tasks run at once, one per processor. Every scheduler round gives
 *        each ready task TIME_SLICE instructions (round robin), then runs the shell.
 *        Tasks that find no free processor wait in the waiting queue.
 * Lab 5: each task sees a logical address space (instructions 0..INSTR_SPACE-1, data
 *        0..DATA_SPACE-1) split into pages; the loader puts each page in a free
 *        physical frame and records it in the task's page table. Frame 0 is reserved.
 * Lab 6: task 1 - finalize() writes <program>_<pid>.freq (executions per source line).
 *        task 2 - page tables live in physical frame 0: processor p's table starts at
 *        PTBR[p] = p * NUM_LOGICAL_PAGES, one byte (a frame number, 0 = unmapped) per page.
 * EXT e21: process life cycle. "xD = fork" (as in e20) creates a child; "exit xR" ends a
 *        task with an exit code (the end of the program = code 0, an error = -1);
 *        "xD = wait [xS]" returns the pid (and exit code) of a child that has ended, and
 *        blocks the caller while its children are still running. A child that ends before
 *        its parent waits is a ZOMBIE until collected; orphans and the zombies of an ended
 *        parent are reaped by the OS. At the end the OS prints every task's exit code.
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

static_assert(NP * NUM_LOGICAL_PAGES <= PAGESIZE, "all page tables must fit in frame 0");
static_assert(NUM_PHYSICAL_PAGES <= 256, "a page-table entry is one byte");

/* EXT e21: BLOCKED = in wait() (keeps its processor, not on the ready queue);
 *          ZOMBIE  = ended, exit code kept until its parent calls wait(); DONE = ended and reaped */
enum TaskState { WAITING, READY, BLOCKED, ZOMBIE, DONE };

struct Task {
    int pid = 0;
    TaskState state = WAITING;
    int proc = -1;                          /* processor while loaded, else -1 */
    std::string program_path, data_path;
    Program prog;                           /* byte code + source map */
    std::vector<unsigned char> data;        /* initial data image */
    long steps = 0;
    int page_faults = 0;
    bool failed = false;
    int ppid = 0;                           /* EXT e21: parent pid, 0 = started by the shell/command line */
    bool forked = false;                    /* EXT e21: start from the copy below, not from program/data */
    CpuState cpu = {};                      /* EXT e21: parent's registers at the fork (child xD = 0) */
    std::vector<unsigned char> saved_page[NUM_LOGICAL_PAGES];   /* EXT e21: copies of the parent's
                                               pages, kept by the OS until the child is loaded; empty = unmapped */
    int exit_code = 0;                      /* EXT e21: from "exit xR"; 0 = program ended; -1 = error */
    int end_seq = 0;                        /* EXT e21: 1 = first task to end, 2 = second, ... */
    int reaped_by = 0;                      /* EXT e21: pid whose wait() collected the code, 0 = the OS */
    int wait_dest = 0, wait_status = -1;    /* EXT e21: registers wait() fills in (-1 = no status) */
};

static std::vector<Task> tasks;             /* tasks[pid - 1] */
static int end_counter = 0;                 /* EXT e21: tasks ended so far */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* pid running on processor p, 0 = free */
static char freePages[NUM_PHYSICAL_PAGES];  /* 1 = allocated; frame 0 holds the page tables */
static int page_faults_on[NP];
static bool shell_open = true;
static std::string shell_buffer;

static int pages_for(size_t bytes) { return (int)((bytes + PAGESIZE - 1) / PAGESIZE); }

/* EXT e21: instruction and data pages a task needs when it is loaded. */
static void page_counts(const Task &t, int &instr, int &data) {
    if (!t.forked) {
        instr = pages_for(t.prog.code.size());
        data = pages_for(t.data.size());
        return;
    }
    instr = data = 0;                       /* a fork child: exactly the pages its parent had */
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++)
        if (!t.saved_page[i].empty()) {
            if (i < NUM_INSTR_PAGES) instr++;
            else data++;
        }
}

static int free_page_count() {
    int n = 0;
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++) n += !freePages[f];
    return n;
}

/* ---------------- memory management unit ---------------- */

int getFreePage() {
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++)
        if (!freePages[f]) { freePages[f] = 1; return f; }
    return -1;
}

int getPhysicalAddress(int p, int isFetch, int address) {
    int limit = isFetch ? INSTR_SPACE : DATA_SPACE;
    if (address < 0 || address >= limit) {
        processor_error("%s address %d is outside the logical %s space 0..%d",
                        isFetch ? "instruction" : "data", address, isFetch ? "instruction" : "data", limit - 1);
        return -1;
    }
    int index = isFetch ? address / PAGESIZE : address / PAGESIZE + INSTR_SPACE / PAGESIZE;
    int pte = PTBR[p] + index;              /* page-table entry, in physical frame 0 */
    int frame = memory[pte];
    if (frame == 0) {                       /* page not mapped */
        if (isFetch) {
            processor_error("fetch from unmapped instruction page %d (jumped past the end of the program?)", index);
            return -1;
        }
        frame = getFreePage();              /* first touch of a data page: give it a zeroed frame */
        if (frame < 0) {
            processor_error("ERROR: no free physical frame for data page %d", index - NUM_INSTR_PAGES);
            return -1;
        }
        std::memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
        memory[pte] = (unsigned char)frame;
        page_faults_on[p]++;
    }
    return frame * PAGESIZE + address % PAGESIZE;
}

/* Put bytes into freshly allocated frames, as logical pages first_index, first_index+1, ... */
static void copy_in(int p, int first_index, const std::vector<unsigned char> &bytes) {
    for (int pg = 0; pg < pages_for(bytes.size()); pg++) {
        int frame = getFreePage();          /* the loader checked that enough are free */
        memory[PTBR[p] + first_index + pg] = (unsigned char)frame;
        size_t start = (size_t)pg * PAGESIZE;
        size_t n = std::min((size_t)PAGESIZE, bytes.size() - start);
        std::memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
        std::memcpy(&memory[frame * PAGESIZE], &bytes[start], n);
    }
}

/* initialize(): load the task's program and data into physical memory, page by page. */
static void initialize(int p, Task &t) {
    PTBR[p] = p * NUM_LOGICAL_PAGES;                       /* this processor's table in frame 0 */
    std::memset(&memory[PTBR[p]], 0, NUM_LOGICAL_PAGES);   /* all pages unmapped */
    if (t.forked) {                                        /* EXT e21: fork child - its parent's pages, */
        for (int i = 0; i < NUM_LOGICAL_PAGES; i++)        /* each copied into a new frame */
            if (!t.saved_page[i].empty()) copy_in(p, i, t.saved_page[i]);
    } else {
        copy_in(p, 0, t.prog.code);                        /* instruction pages 0, 1 */
        copy_in(p, NUM_INSTR_PAGES, t.data);               /* data pages 2..9 */
    }
    page_faults_on[p] = 0;
}

/* Lab 6 task 1: <program>_<pid>.freq, one line per executable source line. */
static std::string write_frequency_file(int p, const Task &t) {
    std::string name = t.prog.name + "_" + std::to_string(t.pid) + ".freq";
    FILE *f = std::fopen(name.c_str(), "w");
    if (!f) return "(could not write " + name + ")";
    const Program &pr = t.prog;
    if (pr.has_source) {
        std::vector<long> count(pr.source.size(), 0);
        std::vector<bool> executable(pr.source.size(), false);
        for (size_t i = 0; i < pr.line_of.size(); i++)
            if (pr.line_of[i] >= 0) {
                count[pr.line_of[i]] += exec_count[p][i];
                executable[pr.line_of[i]] = true;
            }
        for (size_t ln = 0; ln < pr.source.size(); ln++) {
            std::string text = pr.source[ln];
            while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.pop_back();
            if (executable[ln]) std::fprintf(f, "%ld\t%s\n", count[ln], text.c_str());
            else if (pr.is_label[ln]) std::fprintf(f, "%s\n", text.c_str());  /* label: no count */
        }                                                  /* blank and comment-only lines are left out */
    } else {                                               /* started from a .byte file: no source */
        for (size_t i = 0; i + 1 < pr.code.size() / 4; i++)
            std::fprintf(f, "%ld\t%s\n", exec_count[p][i],
                         disassemble(pr.code[4 * i], pr.code[4 * i + 1], pr.code[4 * i + 2], pr.code[4 * i + 3]).c_str());
    }
    std::fclose(f);
    return name;
}

/* finalize(): write the data back, write the frequency report, free the frames. */
static void finalize(int p, Task &t) {
    size_t size = std::max(t.data.size(), (size_t)data_high[p]);
    size = (size + 3) / 4 * 4;
    std::vector<unsigned char> out(size, 0);
    for (size_t a = 0; a < size; a++) {
        int frame = memory[PTBR[p] + NUM_INSTR_PAGES + a / PAGESIZE];
        if (frame) out[a] = memory[frame * PAGESIZE + a % PAGESIZE];
    }
    std::string out_name = file_stem(t.data_path) + "_" + std::to_string(t.pid) + ".out";
    write_byte_file(out_name, out);
    std::string freq_name = write_frequency_file(p, t);

    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {          /* release frames, clear the table */
        int frame = memory[PTBR[p] + i];
        if (frame) freePages[frame] = 0;
        memory[PTBR[p] + i] = 0;
    }
    PTBR[p] = -1;

    t.steps = steps[p];
    t.page_faults = page_faults_on[p];
    t.failed = failed[p];
    std::printf("[os] pid %d (%s) %s on processor %d after %ld instructions, %d page fault(s), exit code %d -> %s, %s\n",
                t.pid, t.prog.name.c_str(), t.failed ? "STOPPED WITH AN ERROR" : "finished", p,
                t.steps, t.page_faults, t.exit_code, out_name.c_str(), freq_name.c_str());   /* EXT e21: + exit code */
}

/* ---------------- loader and tasks ---------------- */

/* loader(): start a waiting task if a processor and enough frames are free. */
static bool loader(int pid) {
    Task &t = tasks[pid - 1];
    int p = -1;
    for (int i = 0; i < NP; i++)
        if (!proc_owner[i]) { p = i; break; }
    int instr_pages = 0, data_pages = 0;                   /* EXT e21: a child needs its parent's pages */
    page_counts(t, instr_pages, data_pages);
    int need = instr_pages + data_pages;
    if (p < 0 || need > free_page_count()) return false;

    proc_owner[p] = pid;
    t.proc = p;
    reset(p);
    current_pid[p] = pid;
    initialize(p, t);
    if (t.forked) load_cpu(p, t.cpu);                      /* EXT e21: continue where the parent forked */
    t.state = READY;
    ready_queue.push_back(pid);
    std::printf("[os] pid %d (%s) loaded on processor %d: %d instruction + %d data page(s), %d frame(s) left\n",
                pid, t.prog.name.c_str(), p, instr_pages, data_pages, free_page_count());
    return true;
}

static void admit_waiting() {
    for (size_t i = 0; i < waiting_queue.size();) {
        if (loader(waiting_queue[i])) waiting_queue.erase(waiting_queue.begin() + (long)i);
        else i++;
    }
}

int map_pid_proc_id(int pid) {
    return (pid >= 1 && pid <= (int)tasks.size()) ? tasks[pid - 1].proc : -1;
}

int failed_task_count() {
    int n = 0;
    for (const Task &t : tasks) n += t.failed;
    return n;
}

int create_task(const std::string &program, const std::string &data) {
    if (tasks.size() >= MAX_TASKS) { std::printf("[os] process table full (%d tasks)\n", MAX_TASKS); return -1; }
    Task t;
    t.pid = (int)tasks.size() + 1;
    t.program_path = program;
    t.data_path = data;
    std::string err;
    bool is_bytecode = program.size() > 5 && program.compare(program.size() - 5, 5, ".byte") == 0;
    bool ok = is_bytecode ? load_bytecode(program, t.prog, err) : compile_file(program, t.prog, err);
    if (ok && !is_bytecode) write_program_byte("program.byte", t.prog);     /* compiled code, as in Lab 1 */
    if (ok) ok = read_byte_file(data, t.data, err);
    if (ok && t.data.size() > (size_t)DATA_SPACE) { err = data + " is larger than the data space (" + std::to_string(DATA_SPACE) + " bytes)"; ok = false; }
    if (ok && pages_for(t.prog.code.size()) + pages_for(t.data.size()) > NUM_PHYSICAL_PAGES - 1) { err = "task needs more frames than physical memory has"; ok = false; }
    if (!ok) { std::printf("[os] cannot start task: %s\n", err.c_str()); return -1; }

    tasks.push_back(t);
    waiting_queue.push_back(t.pid);
    std::printf("[os] pid %d created: %s %s (%zu instructions)\n", t.pid, program.c_str(), data.c_str(), t.prog.code.size() / 4);
    admit_waiting();
    if (tasks.back().state == WAITING) std::printf("[os] pid %d is waiting for a free processor/frames\n", t.pid);
    return t.pid;
}

/* ---------------- EXT e21: fork, wait and exit codes ---------------- */

/* fork() for the task on processor p ("xD = fork", called from execute()), as in e20:
 * the child gets a copy of every mapped page and of the registers and continues after
 * the fork with xD = 0. It is loaded at once if a processor and frames are free, else it waits. */
int os_fork(int p, int dest) {
    int ppid = current_pid[p];
    if (tasks.size() >= MAX_TASKS) {
        std::printf("[os] fork() in pid %d failed: process table full (%d tasks)\n", ppid, MAX_TASKS);
        return -1;
    }
    Task c;
    const Task &parent = tasks[ppid - 1];
    c.pid = (int)tasks.size() + 1;
    c.ppid = ppid;
    c.program_path = parent.program_path;
    c.data_path = parent.data_path;
    c.prog = parent.prog;                                  /* same byte code and source map (.freq) */
    c.data = parent.data;                                  /* initial data image (sizes the .out file) */
    c.forked = true;
    save_cpu(p, c.cpu);                                    /* registers, PC (past the fork), flags */
    c.cpu.x[dest] = 0;                                     /* fork() returns 0 in the child */
    int copied = 0;
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {          /* copy every mapped page out of memory */
        int frame = memory[PTBR[p] + i];
        if (!frame) continue;
        const unsigned char *page = &memory[frame * PAGESIZE];
        c.saved_page[i].assign(page, page + PAGESIZE);
        copied++;
    }
    tasks.push_back(c);                                    /* 'parent' is not used after this */
    waiting_queue.push_back(c.pid);
    std::printf("[os] pid %d created by fork() in pid %d (%d page(s) copied)\n", c.pid, ppid, copied);
    admit_waiting();
    if (tasks[c.pid - 1].state == WAITING) std::printf("[os] pid %d is waiting for a free processor/frames\n", c.pid);
    return c.pid;
}

/* Give an ended child's pid (and exit code) to its parent: the parent is on its processor,
 * so the OS writes straight into the registers that its "xD = wait [xS]" named. */
static void collect(Task &parent, Task &child) {
    int p = parent.proc;
    Register[p][parent.wait_dest] = child.pid;
    if (parent.wait_status >= 0) Register[p][parent.wait_status] = child.exit_code;
    child.state = DONE;
    child.reaped_by = parent.pid;
    std::printf("[os] pid %d: wait() returns pid %d, exit code %d\n", parent.pid, child.pid, child.exit_code);
}

/* wait() for the task on processor p ("xD = wait [xS]", called from execute()):
 *   a child has already ended (zombie) -> collect the one that ended first, no blocking
 *   children still running or waiting -> block: the slice stops, the task leaves the
 *                                        ready queue until a child ends (finish_task)
 *   no children at all                 -> xD = -1 */
void os_wait(int p, int dest, int status_reg) {
    Task &t = tasks[current_pid[p] - 1];
    t.wait_dest = dest;
    t.wait_status = status_reg;
    Task *zombie = nullptr;
    bool running_child = false;
    for (Task &c : tasks) {
        if (c.ppid != t.pid) continue;
        if (c.state == ZOMBIE && (!zombie || c.end_seq < zombie->end_seq)) zombie = &c;
        if (c.state == WAITING || c.state == READY || c.state == BLOCKED) running_child = true;
    }
    if (zombie) { collect(t, *zombie); return; }
    if (!running_child) {
        Register[p][dest] = -1;
        std::printf("[os] pid %d: wait() returns -1 (no children left)\n", t.pid);
        return;
    }
    blocked[p] = 1;                                        /* process_instructions stops the slice */
    t.state = BLOCKED;
    std::printf("[os] pid %d blocks in wait()\n", t.pid);
}

/* ---------------- shell ---------------- */

static void prompt() {
    if (shell_open) { std::printf("$ "); std::fflush(stdout); }
}

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

/* shell(): non-blocking - reads only what is already waiting on stdin. */
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

/* ---------------- scheduler ---------------- */

void os_init(bool batch) {
    std::memset(memory, 0, sizeof memory);
    std::memset(freePages, 0, sizeof freePages);
    freePages[0] = 1;                                  /* frame 0: page tables, never allocated */
    for (int p = 0; p < NP; p++) { PTBR[p] = -1; proc_owner[p] = 0; }
    shell_open = !batch;
}

/* EXT e21: when a task ends, its exit code goes
 *   to its parent at once        if the parent is blocked in wait() (the parent is ready again)
 *   into a ZOMBIE entry          if the parent is still running (until it calls wait())
 *   to the OS                    if it has no parent (shell/command line) or the parent has
 *                                ended (an orphan).
 * Zombie children of the ending task can no longer be collected: the OS reaps them. */
static void finish_task(int pid) {
    Task &t = tasks[pid - 1];
    int p = t.proc;
    t.exit_code = failed[p] ? -1 : exit_code[p];          /* EXT e21 */
    finalize(p, t);
    t.state = DONE;
    t.proc = -1;
    proc_owner[p] = 0;
    blocked[p] = 0;                                        /* EXT e21 (a deadlock victim was blocked) */
    t.end_seq = ++end_counter;                             /* EXT e21: order in which tasks end */

    for (Task &c : tasks)                                  /* EXT e21: its uncollected zombies */
        if (c.ppid == pid && c.state == ZOMBIE) {
            c.state = DONE;
            std::printf("[os] pid %d (zombie child of pid %d) reaped by the os\n", c.pid, pid);
        }
    if (t.ppid == 0) return;                               /* EXT e21: started by the shell: reaped by the os */
    Task &parent = tasks[t.ppid - 1];
    if (parent.state == BLOCKED) {                         /* EXT e21: the parent is waiting for it */
        collect(parent, t);
        blocked[parent.proc] = 0;
        parent.state = READY;
        ready_queue.push_back(parent.pid);                 /* runs again from the next round */
    } else if (parent.state == READY) {                    /* EXT e21: parent alive, not waiting yet */
        t.state = ZOMBIE;
        std::printf("[os] pid %d is a zombie until pid %d calls wait()\n", pid, parent.pid);
    } else {                                               /* EXT e21: parent has ended */
        std::printf("[os] pid %d is an orphan (pid %d has ended): reaped by the os\n", pid, parent.pid);
    }
}

/* EXT e21: deadlock recovery. Nothing is ready and nothing in the waiting queue can be loaded,
 * yet tasks are blocked in wait(): they hold the processors (no context switch here) and frames
 * that the children they wait for need. Stop the newest blocked task (exit code -1) to free them. */
static bool stop_deadlock_victim() {
    int victim = 0;
    for (const Task &t : tasks)
        if (t.state == BLOCKED) victim = t.pid;            /* highest pid = newest */
    if (!victim) return false;
    int p = tasks[victim - 1].proc;
    std::printf("[os] deadlock: blocked tasks hold the processors/frames their children need; stopping pid %d\n", victim);
    failed[p] = 1;
    end_of_simulation[p] = 1;
    finish_task(victim);
    return true;
}

/* EXT e21: ps-like table of how every task ended. */
static void print_exit_table() {
    std::printf("[os] exit codes:\n  pid  ppid  program          state      exit  instructions  reaped by\n");
    for (const Task &t : tasks) {
        bool ran = (t.state != WAITING);
        std::string code = ran ? std::to_string(t.exit_code) : "-";
        std::string by = !ran ? "-" : t.reaped_by ? "pid " + std::to_string(t.reaped_by) + " (wait)" : "os";
        std::printf("  %3d  %4d  %-15s  %-9s  %4s  %12ld  %s\n", t.pid, t.ppid, t.prog.name.c_str(),
                    !ran ? "never ran" : t.failed ? "error" : "exited", code.c_str(), t.steps, by.c_str());
    }
}

void scheduler() {
    if (shell_open) {
        std::printf("[shell] type '<program> <data.byte>' to start a task, 'exit' to stop accepting tasks\n");
        prompt();
    }
    for (;;) {
        admit_waiting();
        size_t n = ready_queue.size();
        for (size_t i = 0; i < n; i++) {               /* one round: every ready task gets a slice */
            int pid = ready_queue.front();
            ready_queue.pop_front();
            int p = map_pid_proc_id(pid);
            process_instructions(p, TIME_SLICE);
            if (end_of_simulation[p]) finish_task(pid);
            else if (!blocked[p]) ready_queue.push_back(pid);   /* EXT e21: blocked in wait(): off the queue */
        }
        shell();
        if (!shell_open && ready_queue.empty()) {
            admit_waiting();
            if (ready_queue.empty() && !stop_deadlock_victim()) break;   /* EXT e21: nothing can run */
        }
        if (ready_queue.empty()) usleep(10000);        /* idle: wait for shell input */
    }
    for (int pid : waiting_queue)
        std::printf("[os] pid %d never started (not enough free frames)\n", pid);
    if (!tasks.empty()) print_exit_table();            /* EXT e21 */
    std::printf("[os] all tasks finished; print output is in %s\n", LOG_FILE);
    if (fd_log) { std::fclose(fd_log); fd_log = nullptr; }
}
