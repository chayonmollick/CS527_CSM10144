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
 * EXT e17: shell commands ps, kill <pid>, mem, help (and sleep <n> for scripted tests);
 *        an unknown command prints the help text.
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

enum TaskState { WAITING, READY, DONE, KILLED };    /* EXT e17: KILLED = stopped by 'kill' */

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
    bool killed = false;                    /* EXT e17: stopped by the shell's kill command */
};

static std::vector<Task> tasks;             /* tasks[pid - 1] */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* pid running on processor p, 0 = free */
static char freePages[NUM_PHYSICAL_PAGES];  /* 1 = allocated; frame 0 holds the page tables */
static int page_faults_on[NP];
static bool shell_open = true;
static std::string shell_buffer;
static bool input_ended = false;            /* EXT e17: stdin reached end of file */
static int shell_sleep = 0;                 /* EXT e17: rounds left before the next command ('sleep n') */

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
    copy_in(p, 0, t.prog.code);                            /* instruction pages 0, 1 */
    copy_in(p, NUM_INSTR_PAGES, t.data);                   /* data pages 2..9 */
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
    std::printf("[os] pid %d (%s) %s on processor %d after %ld instructions, %d page fault(s) -> %s, %s\n",
                t.pid, t.prog.name.c_str(),
                t.killed ? "KILLED" : t.failed ? "STOPPED WITH AN ERROR" : "finished", p,   /* EXT e17 */
                t.steps, t.page_faults, out_name.c_str(), freq_name.c_str());
}

/* ---------------- loader and tasks ---------------- */

/* loader(): start a waiting task if a processor and enough frames are free. */
static bool loader(int pid) {
    Task &t = tasks[pid - 1];
    int p = -1;
    for (int i = 0; i < NP; i++)
        if (!proc_owner[i]) { p = i; break; }
    int need = pages_for(t.prog.code.size()) + pages_for(t.data.size());
    if (p < 0 || need > free_page_count()) return false;

    proc_owner[p] = pid;
    t.proc = p;
    reset(p);
    current_pid[p] = pid;
    initialize(p, t);
    t.state = READY;
    ready_queue.push_back(pid);
    std::printf("[os] pid %d (%s) loaded on processor %d: %d instruction + %d data page(s), %d frame(s) left\n",
                pid, t.prog.name.c_str(), p, pages_for(t.prog.code.size()), pages_for(t.data.size()), free_page_count());
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

/* ---------------- shell ---------------- */

static void prompt() {
    if (shell_open) { std::printf("$ "); std::fflush(stdout); }
}

/* ---- EXT e17: built-in shell commands ps, kill, mem, help, sleep ---- */

static void finish_task(int pid);                      /* EXT e17: defined with the scheduler below */

static const char *state_name(const Task &t) {         /* EXT e17 */
    switch (t.state) {
    case WAITING: return "WAITING";
    case READY:   return "READY";
    case KILLED:  return "KILLED";
    default:      return t.failed ? "FAILED" : "DONE";
    }
}

static int frames_held(int p) {                        /* EXT e17: mapped entries of p's page table */
    int n = 0;
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) n += memory[PTBR[p] + i] != 0;
    return n;
}

/* EXT e17: ps - one line per task: pid, program, state, processor, instructions so far, frames held */
static void ps() {
    std::printf("  PID  PROGRAM         STATE    PROC   INSTR  PAGES\n");
    for (const Task &t : tasks) {
        int p = t.proc;                                /* >= 0 only while the task is loaded */
        char proc[16] = "-";
        if (p >= 0) std::snprintf(proc, sizeof proc, "%d", p);
        std::printf("%5d  %-14s  %-7s  %4s  %6ld  %5d\n", t.pid, t.prog.name.c_str(), state_name(t), proc,
                    p >= 0 ? steps[p] : t.steps, p >= 0 ? frames_held(p) : 0);
    }
}

/* EXT e17: mem - free frames, and which pid owns each frame (read from the page tables in frame 0) */
static void mem() {
    int owner[NUM_PHYSICAL_PAGES] = {0};
    for (int p = 0; p < NP; p++)
        if (proc_owner[p])
            for (int i = 0; i < NUM_LOGICAL_PAGES; i++)
                if (memory[PTBR[p] + i]) owner[memory[PTBR[p] + i]] = proc_owner[p];
    std::printf("[mem] %d of %d frames free (frame 0 holds the page tables)\n", free_page_count(), NUM_PHYSICAL_PAGES - 1);
    std::printf("[mem] frame");
    for (int f = 0; f < NUM_PHYSICAL_PAGES; f++) std::printf(" %3d", f);
    std::printf("\n[mem] owner  PT");
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++) {
        if (owner[f]) std::printf(" %3d", owner[f]);
        else std::printf("   .");
    }
    std::printf("\n");
}

/* EXT e17: kill <pid> - stop a running or waiting task. Its output is still written. */
static void kill_task(int pid) {
    if (pid < 1 || pid > (int)tasks.size()) { std::printf("[shell] kill: no task with pid %d\n", pid); return; }
    Task &t = tasks[pid - 1];
    if (t.state == READY) {                            /* on a processor: drop it from the ready queue */
        auto it = std::find(ready_queue.begin(), ready_queue.end(), pid);
        if (it != ready_queue.end()) ready_queue.erase(it);
        t.killed = true;
        finish_task(pid);                              /* writes .out and .freq, frees frames and processor */
    } else if (t.state == WAITING) {                   /* never ran: its output is its initial data */
        auto it = std::find(waiting_queue.begin(), waiting_queue.end(), pid);
        if (it != waiting_queue.end()) waiting_queue.erase(it);
        t.killed = true;
        t.state = KILLED;
        std::vector<unsigned char> out = t.data;
        out.resize((out.size() + 3) / 4 * 4, 0);
        std::string out_name = file_stem(t.data_path) + "_" + std::to_string(t.pid) + ".out";
        write_byte_file(out_name, out);
        std::printf("[os] pid %d (%s) KILLED while waiting (it never ran) -> %s\n", t.pid, t.prog.name.c_str(), out_name.c_str());
    } else {
        std::printf("[shell] kill: pid %d is not running or waiting (it is %s)\n", pid, state_name(t));
    }
}

static void help() {                                   /* EXT e17 */
    std::printf("[shell] usage: <program.txt|program.byte> <data.byte>   start a task\n"
                "        ps            list tasks: pid, program, state, processor, instructions, pages\n"
                "        kill <pid>    stop a running or waiting task (its output is still written)\n"
                "        mem           free frames and the owner of every frame\n"
                "        sleep <n>     read the next command after n scheduler rounds\n"
                "        help          this text\n"
                "        exit          accept no new tasks; finish the running ones\n");
}

static bool to_number(const std::string &s, int &v) {  /* EXT e17: "12" -> 12; false if not 0..999999 */
    if (s.empty() || s.size() > 6 || s.find_first_not_of("0123456789") != std::string::npos) return false;
    v = std::stoi(s);
    return true;
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
    int n = 0;                                         /* EXT e17: built-in commands first */
    if (words[0] == "ps") ps();
    else if (words[0] == "mem") mem();
    else if (words[0] == "help") help();
    else if (words[0] == "kill") {
        if (words.size() == 2 && to_number(words[1], n)) kill_task(n);
        else std::printf("[shell] kill: give one pid, e.g. 'kill 2'\n");
    } else if (words[0] == "sleep") {
        if (words.size() == 2 && to_number(words[1], n)) {
            shell_sleep = n;
            std::printf("[shell] sleep: next command after %d round(s)\n", n);
        } else std::printf("[shell] sleep: give a number of rounds, e.g. 'sleep 3'\n");
    }
    else if (words.size() == 2) create_task(words[0], words[1]);
    else {                                             /* EXT e17: unknown command -> help */
        std::printf("[shell] unknown command '%s'\n", words[0].c_str());
        help();
    }
    prompt();
}

/* shell(): non-blocking - reads only what is already waiting on stdin. */
void shell() {
    if (!shell_open) return;
    while (!input_ended) {                             /* take whatever is already typed */
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);
        timeval no_wait = {0, 0};
        if (select(1, &fds, nullptr, nullptr, &no_wait) <= 0) break;
        char buf[256];
        ssize_t n = read(0, buf, sizeof buf);
        if (n <= 0) {                                  /* EXT e17: remember end of input; lines may */
            input_ended = true;                        /* still be queued behind a 'sleep'          */
            if (!shell_buffer.empty() && shell_buffer.back() != '\n') shell_buffer += '\n';
            break;
        }
        shell_buffer.append(buf, (size_t)n);
    }
    if (shell_sleep > 0 && --shell_sleep > 0) return;  /* EXT e17: 'sleep n' - n rounds without commands */
    size_t nl;                                         /* run every complete line */
    while (shell_open && shell_sleep == 0 && (nl = shell_buffer.find('\n')) != std::string::npos) {
        std::string line = shell_buffer.substr(0, nl);
        shell_buffer.erase(0, nl + 1);
        run_command(line);
    }
    if (input_ended && shell_open && shell_sleep == 0) {   /* end of input acts like "exit" (EXT e17: */
        std::printf("\n[shell] end of input: no new tasks\n");   /* once every queued line has run) */
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

static void finish_task(int pid) {
    Task &t = tasks[pid - 1];
    int p = t.proc;
    finalize(p, t);
    t.state = t.killed ? KILLED : DONE;                /* EXT e17 */
    t.proc = -1;
    proc_owner[p] = 0;
}

void scheduler() {
    if (shell_open) {
        std::printf("[shell] type '<program> <data.byte>' to start a task, 'help' for ps/kill/mem, "
                    "'exit' to stop accepting tasks\n");      /* EXT e17 */
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
    std::printf("[os] all tasks finished; print output is in %s\n", LOG_FILE);
    if (fd_log) { std::fclose(fd_log); fd_log = nullptr; }
}
