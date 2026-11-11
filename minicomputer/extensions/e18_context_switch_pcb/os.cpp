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
 * EXT e18: context switching. A task no longer owns a processor until it ends. Every resident
 *        task has a PCB (struct PCB in processor.h) and a page-table slot s in frame 0
 *        (table at s * NUM_LOGICAL_PAGES). For every time slice the scheduler dispatches the
 *        next ready task: save the processor's old task into its PCB, restore the new task's
 *        PCB - registers, PC, flags, counters and PTBR = its own page table. So more tasks
 *        than processors can run at once (NP = 1 in config.h); memory is the only limit.
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

static_assert(NUM_PT_SLOTS * NUM_LOGICAL_PAGES <= PAGESIZE, "all page tables must fit in frame 0");  /* EXT e18 */
static_assert(NUM_PHYSICAL_PAGES <= 256, "a page-table entry is one byte");

enum TaskState { WAITING, READY, DONE };

struct Task {
    int pid = 0;
    TaskState state = WAITING;
    int proc = -1;                          /* EXT e18: processor holding its registers right now, else -1 */
    std::string program_path, data_path;
    Program prog;                           /* byte code + source map */
    std::vector<unsigned char> data;        /* initial data image */
    long steps = 0;
    int page_faults = 0;                    /* EXT e18: counted per task (was per processor) */
    bool failed = false;
    PCB pcb = {};                           /* EXT e18: saved state while off the processor */
    int slot = -1;                          /* EXT e18: page-table slot in frame 0, -1 = not resident */
    int slices = 0;                         /* EXT e18: time slices received */
    int switches = 0;                       /* EXT e18: times its PCB was restored onto a processor */
};

static std::vector<Task> tasks;             /* tasks[pid - 1] */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* EXT e18: pid whose registers are on processor p, 0 = none */
static int slot_owner[NUM_PT_SLOTS];        /* EXT e18: pid using page-table slot s, 0 = free */
static long context_switches = 0;           /* EXT e18: total for the run */
static char freePages[NUM_PHYSICAL_PAGES];  /* 1 = allocated; frame 0 holds the page tables */
static bool shell_open = true;
static std::string shell_buffer;

static int pages_for(size_t bytes) { return (int)((bytes + PAGESIZE - 1) / PAGESIZE); }

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
        tasks[current_pid[p] - 1].page_faults++;   /* EXT e18: the task, not the processor */
    }
    return frame * PAGESIZE + address % PAGESIZE;
}

/* Put bytes into freshly allocated frames, as logical pages first_index, first_index+1, ...
 * EXT e18: ptbr = address of the task's page table in frame 0 (no processor is involved). */
static void copy_in(int ptbr, int first_index, const std::vector<unsigned char> &bytes) {
    for (int pg = 0; pg < pages_for(bytes.size()); pg++) {
        int frame = getFreePage();          /* the loader checked that enough are free */
        memory[ptbr + first_index + pg] = (unsigned char)frame;
        size_t start = (size_t)pg * PAGESIZE;
        size_t n = std::min((size_t)PAGESIZE, bytes.size() - start);
        std::memset(&memory[frame * PAGESIZE], 0, PAGESIZE);
        std::memcpy(&memory[frame * PAGESIZE], &bytes[start], n);
    }
}

/* initialize(): load the task's program and data into physical memory, page by page.
 * EXT e18: the page table belongs to the task (its slot), not to a processor; the PCB starts
 * cleared (what reset() did to a processor) with ptbr = the slot's address. */
static void initialize(Task &t) {
    t.pcb = PCB();                                         /* x, v, PC, flags, counters = 0 */
    t.pcb.ptbr = t.slot * NUM_LOGICAL_PAGES;               /* this task's table in frame 0 */
    std::memset(&memory[t.pcb.ptbr], 0, NUM_LOGICAL_PAGES);   /* all pages unmapped */
    copy_in(t.pcb.ptbr, 0, t.prog.code);                   /* instruction pages 0, 1 */
    copy_in(t.pcb.ptbr, NUM_INSTR_PAGES, t.data);          /* data pages 2..9 */
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
    t.failed = failed[p];                                  /* EXT e18: page faults are counted per task */
    std::printf("[os] pid %d (%s) %s on processor %d after %ld instructions, %d page fault(s), "
                "%d slice(s), %d context switch(es) -> %s, %s\n",          /* EXT e18: slices, switches */
                t.pid, t.prog.name.c_str(), t.failed ? "STOPPED WITH AN ERROR" : "finished", p,
                t.steps, t.page_faults, t.slices, t.switches, out_name.c_str(), freq_name.c_str());
}

/* ---------------- loader and tasks ---------------- */

/* loader(): make a waiting task resident if a page-table slot and enough frames are free.
 * EXT e18: it no longer needs a free processor - the processors are shared by context switching. */
static bool loader(int pid) {
    Task &t = tasks[pid - 1];
    int s = -1;
    for (int i = 0; i < NUM_PT_SLOTS; i++)
        if (!slot_owner[i]) { s = i; break; }
    int need = pages_for(t.prog.code.size()) + pages_for(t.data.size());
    if (s < 0 || need > free_page_count()) return false;

    slot_owner[s] = pid;
    t.slot = s;
    initialize(t);
    if (!fd_log) fd_log = std::fopen(LOG_FILE, "a");       /* reset() used to open it */
    t.state = READY;
    ready_queue.push_back(pid);
    std::printf("[os] pid %d (%s) loaded: page table in slot %d (PTBR = %d), %d instruction + %d data page(s), "
                "%d frame(s) left\n", pid, t.prog.name.c_str(), s, t.pcb.ptbr,
                pages_for(t.prog.code.size()), pages_for(t.data.size()), free_page_count());
    return true;
}

/* EXT e18: dispatch(): context switch - give processor p to task pid for its next time slice.
 * The processor's previous task is saved into its PCB, then pid's PCB is restored, which also
 * sets PTBR[p] to pid's own page table. Nothing to do if pid is still on p from its last slice. */
static void dispatch(int p, int pid) {
    if (proc_owner[p] == pid) return;
    int old = proc_owner[p];
    if (old) {                                             /* save the task leaving the processor */
        save_context(p, tasks[old - 1].pcb);
        tasks[old - 1].proc = -1;
    }
    Task &t = tasks[pid - 1];
    if (t.proc >= 0) {                                     /* (NP > 1) still on another processor */
        save_context(t.proc, t.pcb);
        proc_owner[t.proc] = 0;
        PTBR[t.proc] = -1;
    }
    restore_context(p, t.pcb);                             /* registers, PC, flags, counters, PTBR */
    current_pid[p] = pid;
    proc_owner[p] = pid;
    t.proc = p;
    t.switches++;
    context_switches++;
    if (trace) {                                           /* -t also shows every switch */
        char from[16] = "idle";
        if (old) std::snprintf(from, sizeof from, "pid %d", old);
        std::fprintf(stderr, "[os] context switch on processor %d: %s -> pid %d (PTBR = %d)\n", p, from, pid, PTBR[p]);
    }
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
    if (tasks.back().state == WAITING)                     /* EXT e18: only memory can make it wait */
        std::printf("[os] pid %d is waiting for free frames/page-table slot\n", t.pid);
    return t.pid;
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
    std::memset(slot_owner, 0, sizeof slot_owner);     /* EXT e18: every page-table slot free */
    shell_open = !batch;
}

static void finish_task(int pid) {
    Task &t = tasks[pid - 1];
    int p = t.proc;                                    /* it has just run its last slice on p */
    finalize(p, t);
    t.state = DONE;
    t.proc = -1;
    proc_owner[p] = 0;
    slot_owner[t.slot] = 0;                            /* EXT e18: its page-table slot is free again */
    t.slot = -1;
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
            int p = (int)(i % NP);                     /* EXT e18: processors take turns (NP = 1: always 0) */
            dispatch(p, pid);                          /* EXT e18: context switch if p held another task */
            tasks[pid - 1].slices++;
            process_instructions(p, TIME_SLICE);
            if (end_of_simulation[p]) finish_task(pid);
            else ready_queue.push_back(pid);
        }
        shell();
        if (!shell_open && ready_queue.empty()) {
            admit_waiting();
            if (ready_queue.empty()) break;            /* nothing running, nothing can start */
        }
        if (ready_queue.empty()) usleep(10000);        /* idle: wait for shell input */
    }
    for (int pid : waiting_queue)
        std::printf("[os] pid %d never started (not enough free frames)\n", pid);
    std::printf("[os] %ld context switch(es) on %d processor(s)\n", context_switches, NP);   /* EXT e18 */
    std::printf("[os] all tasks finished; print output is in %s\n", LOG_FILE);
    if (fd_log) { std::fclose(fd_log); fd_log = nullptr; }
}
