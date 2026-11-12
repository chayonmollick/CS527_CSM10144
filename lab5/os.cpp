/* Operating system (handout: Operating System (OS)): tasks, scheduling, shell, loader and
 * the memory management unit.
 *
 * Scheduler, Shell, Loader (unchanged since Lab 4): up to NP tasks run at once, one per
 *     processor. Every scheduler round gives each ready task TIME_SLICE instructions (round
 *     robin), then runs the shell. A task that finds no free processor waits.
 * Memory management unit (Lab 5): each task sees a logical instruction space
 *     (0..INSTR_SPACE-1) and a logical data space (0..DATA_SPACE-1), both split into pages.
 *     The OS keeps a page table per processor, pageTable[p][NUM_LOGICAL_PAGES], whose entries
 *     hold frame numbers, and the free-frame list freePages[]. initialize() puts every page
 *     of the program and of the data file into a free frame and writes the page table;
 *     finalize() writes the data back, frees the frames and resets the page table.
 *     Frame 0 is reserved: it is never allocated, so a page-table entry 0 means "not mapped".
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

static_assert(INSTR_SPACE % PAGESIZE == 0 && DATA_SPACE % PAGESIZE == 0 && MEMSIZE % PAGESIZE == 0,
              "the logical spaces and physical memory must be whole pages");
static_assert(NUM_PHYSICAL_PAGES <= 128, "a page-table entry is a char: frame numbers must fit in 0..127");

enum TaskState { WAITING, READY, DONE };

struct Task {
    int pid = 0;
    TaskState state = WAITING;
    int proc = -1;                          /* processor while loaded, else -1 */
    std::string program_path, data_path;
    Program prog;                           /* byte code */
    std::vector<unsigned char> data;        /* initial data image (the data.byte file) */
    long steps = 0;
    int page_faults = 0;
    bool failed = false;
};

static std::vector<Task> tasks;             /* tasks[pid - 1] */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* pid running on processor p, 0 = free */
static int page_faults_on[NP];              /* data pages mapped on first touch, per processor */
static bool shell_open = true;
static std::string shell_buffer;

/* ---------------- memory management unit (handout: OS; Memory management related functions) ---------------- */

/* The handout's char pageTable[MAX_PROC][NUM_LOGICAL_PAGES], with one table per processor
 * (MAX_PROC = NP). A task gets a processor in loader() and keeps it until finalize(), and only
 * a task that is on a processor owns frames (a waiting task is not loaded yet). So the
 * processor id names the page table of the task running there, and it is exactly what
 * getPhysicalAddress(proc_id, ...) receives.
 * Entries 0..NUM_INSTR_PAGES-1 belong to the instruction pages, the rest to the data pages.
 * An entry holds a frame number; 0 = not mapped. */
static char pageTable[NP][NUM_LOGICAL_PAGES];

/* char freePages[NUM_PHYSICAL_PAGES]: 0 = free, 1 = allocated. freePages[0] is always 1. */
static char freePages[NUM_PHYSICAL_PAGES];

static int pages_for(size_t bytes) { return (int)((bytes + PAGESIZE - 1) / PAGESIZE); }

static int free_page_count() {
    int n = 0;
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++) n += !freePages[f];
    return n;
}

/* getFreePage(): go through the free-frame list, return the first free frame and mark it
 * allocated. Frame 0 is reserved and never returned. -1 when no frame is free; the caller
 * prints the ERROR (see getPhysicalAddress). */
int getFreePage() {
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++)
        if (!freePages[f]) { freePages[f] = 1; return f; }
    return -1;
}

/* getPhysicalAddress(proc_id, isFetch, address): logical -> physical address.
 *     index    = isFetch ? address / PAGESIZE : address / PAGESIZE + 1024 / PAGESIZE
 *     frame    = pageTable[proc_id][index]
 *     physical = frame * PAGESIZE + address % PAGESIZE
 * A data page the data file did not reach is not mapped yet: on its first touch it gets a
 * zeroed frame (a "page fault", counted per task). */
int getPhysicalAddress(int p, int isFetch, int address) {
    int limit = isFetch ? INSTR_SPACE : DATA_SPACE;
    if (address < 0 || address >= limit) {
        processor_error("%s address %d is outside the logical %s space 0..%d",
                        isFetch ? "instruction" : "data", address, isFetch ? "instruction" : "data", limit - 1);
        return -1;
    }
    int index = isFetch ? address / PAGESIZE : address / PAGESIZE + INSTR_SPACE / PAGESIZE;
    int frame = pageTable[p][index];
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
        pageTable[p][index] = (char)frame;
        page_faults_on[p]++;
    }
    return frame * PAGESIZE + address % PAGESIZE;
}

/* "1 - | 2 3 - 4 - - - 5": the frame of every logical page, instruction pages first, then
 * the data pages; "-" = not mapped. */
static std::string page_table_text(int p) {
    std::string s;
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {
        if (i == NUM_INSTR_PAGES) s += "| ";
        s += pageTable[p][i] ? std::to_string(pageTable[p][i]) : std::string("-");
        if (i + 1 < NUM_LOGICAL_PAGES) s += " ";
    }
    return s;
}

/* Copy a file image into physical memory page by page: each page gets a frame from
 * getFreePage(), and the frame number goes into page-table entry first_index + page. */
static void load_pages(int p, int first_index, const std::vector<unsigned char> &bytes) {
    for (int pg = 0; pg < pages_for(bytes.size()); pg++) {    /* how many PAGES the file has */
        int frame = getFreePage();
        if (frame < 0) return;                                /* cannot happen: the loader checked the free frames */
        pageTable[p][first_index + pg] = (char)frame;         /* update the page table */
        size_t start = (size_t)pg * PAGESIZE;
        size_t n = std::min((size_t)PAGESIZE, bytes.size() - start);
        std::memset(&memory[frame * PAGESIZE], 0, PAGESIZE); /* the rest of a last, partial page is 0 */
        std::memcpy(&memory[frame * PAGESIZE], &bytes[start], n);
    }
}

/* initialize() (handout): load the task's program and data into physical memory, frame by
 * frame, and write its page table. */
static void initialize(int p, Task &t) {
    std::memset(pageTable[p], 0, sizeof pageTable[p]);    /* all pages unmapped */
    load_pages(p, 0, t.prog.code);                        /* program.byte -> instruction pages, entries 0..1 */
    load_pages(p, NUM_INSTR_PAGES, t.data);               /* data.byte -> data pages, entries 2..9 */
    page_faults_on[p] = 0;
}

/* finalize() (handout): write the data back, reset the page table, free the task's frames. */
static void finalize(int p, Task &t) {
    /* Write data.byte: read the task's data space back through its page table. The output is
     * as long as data.byte or reaches the highest address the task wrote, whichever is more. */
    size_t size = std::max(t.data.size(), (size_t)data_high[p]);
    size = (size + 3) / 4 * 4;
    std::vector<unsigned char> out(size, 0);
    for (size_t a = 0; a < size; a++) {
        int frame = pageTable[p][NUM_INSTR_PAGES + a / PAGESIZE];
        if (frame) out[a] = memory[frame * PAGESIZE + a % PAGESIZE];
    }
    std::string out_name = file_stem(t.data_path) + "_" + std::to_string(t.pid) + ".out";
    write_byte_file(out_name, out);

    t.steps = steps[p];
    t.page_faults = page_faults_on[p];
    t.failed = failed[p];
    std::printf("[os] pid %d (%s) %s on processor %d after %ld instructions, %d page fault(s) -> %s\n",
                t.pid, t.prog.name.c_str(), t.failed ? "STOPPED WITH AN ERROR" : "finished", p,
                t.steps, t.page_faults, out_name.c_str());

    /* Reset the page table and give its frames back to freePages[]. */
    std::string table = page_table_text(p);
    int freed = 0;
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {
        int frame = pageTable[p][i];
        if (frame) { freePages[frame] = 0; freed++; }
        pageTable[p][i] = 0;
    }
    std::printf("[os]     page table [%s] reset: %d frame(s) freed, %d free\n",
                table.c_str(), freed, free_page_count());
}

/* ---------------- loader and tasks (handout: Loader, Scheduler) ---------------- */

static int free_processor() {
    for (int p = 0; p < NP; p++)
        if (!proc_owner[p]) return p;
    return -1;
}

/* Frames the loader must find: one per page of program.byte and of data.byte. */
static int frames_needed(const Task &t) {
    return pages_for(t.prog.code.size()) + pages_for(t.data.size());
}

/* loader(): give a waiting task a free processor (PID -> processor map), reset that processor,
 * load the task's pages with initialize() and put the task in the ready queue.
 * Lab 5: it starts the task only if its pages also fit in the free frames; otherwise the task
 * keeps waiting, just as it waits for a processor. */
static bool loader(int pid) {
    Task &t = tasks[pid - 1];
    int p = free_processor();
    if (p < 0 || frames_needed(t) > free_page_count()) return false;

    proc_owner[p] = pid;
    t.proc = p;
    reset(p);
    current_pid[p] = pid;
    initialize(p, t);
    t.state = READY;
    ready_queue.push_back(pid);
    std::printf("[os] pid %d (%s) loaded on processor %d: page table [%s], %d frame(s) free\n",
                pid, t.prog.name.c_str(), p, page_table_text(p).c_str(), free_page_count());
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
    if (ok && frames_needed(t) > NUM_PHYSICAL_PAGES - 1) { err = "task needs more frames than physical memory has"; ok = false; }
    if (!ok) { std::printf("[os] cannot start task: %s\n", err.c_str()); return -1; }

    tasks.push_back(t);
    waiting_queue.push_back(t.pid);
    std::printf("[os] pid %d created: %s %s (%zu instructions)\n", t.pid, program.c_str(), data.c_str(), t.prog.code.size() / 4);
    admit_waiting();
    const Task &added = tasks.back();
    if (added.state == WAITING) {
        if (free_processor() < 0) std::printf("[os] pid %d is waiting: no free processor\n", added.pid);
        else std::printf("[os] pid %d is waiting: needs %d frame(s), %d free\n",
                         added.pid, frames_needed(added), free_page_count());
    }
    return t.pid;
}

/* ---------------- shell (handout: Shell) ---------------- */

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

/* ---------------- scheduler (handout: Scheduler) ---------------- */

void os_init(bool batch) {
    std::memset(memory, 0, sizeof memory);
    std::memset(pageTable, 0, sizeof pageTable);       /* no page mapped */
    std::memset(freePages, 0, sizeof freePages);       /* every frame free ... */
    freePages[0] = 1;                                  /* ... except frame 0: reserved, never allocated */
    for (int p = 0; p < NP; p++) proc_owner[p] = 0;
    shell_open = !batch;
}

static void finish_task(int pid) {
    Task &t = tasks[pid - 1];
    int p = t.proc;
    finalize(p, t);
    t.state = DONE;
    t.proc = -1;
    proc_owner[p] = 0;
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
            int p = map_pid_proc_id(pid);              /* the handout passes pid; the processor runs it */
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
    std::printf("[os] all tasks finished; %d of %d frames free; print output is in %s\n",
                free_page_count(), NUM_PHYSICAL_PAGES - 1, LOG_FILE);
    if (fd_log) { std::fclose(fd_log); fd_log = nullptr; }
}
