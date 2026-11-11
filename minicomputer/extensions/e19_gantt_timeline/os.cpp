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
 * EXT e19: the scheduler records, for every round, which pid ran on each processor; at exit
 *        timeline.txt gets a text Gantt chart, per-task arrival/start/finish rounds with
 *        waiting and turnaround times, and the utilisation of every processor.
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

enum TaskState { WAITING, READY, DONE };

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
    int arrival = 0, start = 0, finish = 0; /* EXT e19: rounds: created, first slice, last slice (0 = not yet) */
    int run_rounds = 0;                     /* EXT e19: rounds in which it got a slice */
};

static std::vector<Task> tasks;             /* tasks[pid - 1] */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* pid running on processor p, 0 = free */
static char freePages[NUM_PHYSICAL_PAGES];  /* 1 = allocated; frame 0 holds the page tables */
static int page_faults_on[NP];
static bool shell_open = true;
static std::string shell_buffer;

static int round_no = 0;                    /* EXT e19: rounds in which some task ran (round 1 = first) */
static std::vector<int> gantt;              /* EXT e19: gantt[(r - 1) * NP + p] = pid that ran on processor p
                                               in round r, 0 = idle (NP entries per round) */

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
                t.pid, t.prog.name.c_str(), t.failed ? "STOPPED WITH AN ERROR" : "finished", p,
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
    t.arrival = round_no;                                  /* EXT e19: 0 for command-line tasks */
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

static void finish_task(int pid) {
    Task &t = tasks[pid - 1];
    int p = t.proc;
    finalize(p, t);
    t.state = DONE;
    t.proc = -1;
    proc_owner[p] = 0;
    t.finish = round_no;                               /* EXT e19 */
}

/* EXT e19: timeline.txt = Gantt chart (rows = processors, columns = rounds, cell = pid or '.'),
 * then per-task times and processor utilisation. Times are in rounds:
 *   turnaround = finish - arrival,  waiting = turnaround - run_rounds (in the system, not running). */
static void write_timeline() {
    FILE *f = std::fopen(TIMELINE_FILE, "w");
    if (!f) { std::printf("[os] could not write %s\n", TIMELINE_FILE); return; }
    int w = 2;                                         /* cell width = digits of the largest number + 1 */
    for (int v = std::max(round_no, (int)tasks.size()); v >= 10; v /= 10) w++;
    std::fprintf(f, "Gantt chart: pid that ran on each processor in each round ('.' = idle)\n");
    std::fprintf(f, "NP = %d processors, TIME_SLICE = %d instructions, %d rounds\n", NP, TIME_SLICE, round_no);
    for (int first = 1; first <= round_no; first += GANTT_COLUMNS) {   /* blocks of GANTT_COLUMNS rounds */
        int last = std::min(round_no, first + GANTT_COLUMNS - 1);
        std::fprintf(f, "\nround");
        for (int r = first; r <= last; r++) std::fprintf(f, "%*d", w, r);
        std::fprintf(f, "\n");
        for (int p = 0; p < NP; p++) {
            std::fprintf(f, "P%-4d", p);
            for (int r = first; r <= last; r++) {
                int pid = gantt[(size_t)(r - 1) * NP + p];
                if (pid) std::fprintf(f, "%*d", w, pid);
                else std::fprintf(f, "%*s", w, ".");
            }
            std::fprintf(f, "\n");
        }
    }

    std::fprintf(f, "\npid program          arrival start finish  run waiting turnaround\n");
    int n = 0;
    double sum_wait = 0, sum_turn = 0;
    for (const Task &t : tasks) {
        if (!t.finish) continue;                       /* never ran */
        int turnaround = t.finish - t.arrival, waiting = turnaround - t.run_rounds;
        std::fprintf(f, "%3d %-16s %7d %5d %6d %4d %7d %10d\n", t.pid, t.prog.name.c_str(), t.arrival,
                     t.start, t.finish, t.run_rounds, waiting, turnaround);
        sum_wait += waiting;
        sum_turn += turnaround;
        n++;
    }
    if (n) std::fprintf(f, "average waiting %.2f rounds, average turnaround %.2f rounds (%d tasks)\n",
                        sum_wait / n, sum_turn / n, n);

    std::fprintf(f, "\nprocessor utilisation = busy rounds / %d rounds\n", round_no);
    int all_busy = 0;
    for (int p = 0; p < NP; p++) {
        int busy = 0;
        for (int r = 1; r <= round_no; r++) busy += gantt[(size_t)(r - 1) * NP + p] != 0;
        all_busy += busy;
        std::fprintf(f, "P%-4d %4d / %-5d %5.1f %%\n", p, busy, round_no, 100.0 * busy / round_no);
    }
    std::fprintf(f, "all   %4d / %-5d %5.1f %%\n", all_busy, round_no * NP, 100.0 * all_busy / (round_no * NP));
    std::fclose(f);
    std::printf("[os] Gantt chart, task times and processor utilisation written to %s\n", TIMELINE_FILE);
}

void scheduler() {
    if (shell_open) {
        std::printf("[shell] type '<program> <data.byte>' to start a task, 'exit' to stop accepting tasks\n");
        prompt();
    }
    for (;;) {
        admit_waiting();
        if (!ready_queue.empty()) {                    /* EXT e19: a new round = one more Gantt column */
            round_no++;                                /* (idle waits for shell input are not rounds) */
            gantt.resize(gantt.size() + NP, 0);        /* every processor idle until it runs a slice */
        }
        size_t n = ready_queue.size();
        for (size_t i = 0; i < n; i++) {               /* one round: every ready task gets a slice */
            int pid = ready_queue.front();
            ready_queue.pop_front();
            int p = map_pid_proc_id(pid);
            gantt[(size_t)(round_no - 1) * NP + p] = pid;                  /* EXT e19 */
            if (!tasks[pid - 1].start) tasks[pid - 1].start = round_no;    /* EXT e19: first slice */
            tasks[pid - 1].run_rounds++;                                   /* EXT e19 */
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
    if (round_no > 0) write_timeline();                /* EXT e19 */
    std::printf("[os] all tasks finished; print output is in %s\n", LOG_FILE);
    if (fd_log) { std::fclose(fd_log); fd_log = nullptr; }
}
