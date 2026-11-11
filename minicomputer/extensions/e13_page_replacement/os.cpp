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
 * EXT e13: demand paging with page replacement. The loader needs only the instruction
 *        pages + 1 data page in memory; the rest of the data image waits in a backing
 *        store. When no frame is free, a page fault evicts a victim data page (FIFO or
 *        LRU, local or global), saves it to the backing store and reuses its frame.
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
    int page_ins = 0, page_outs = 0;        /* EXT e13: pages read from / written to the backing store */
    bool failed = false;
};

static std::vector<Task> tasks;             /* tasks[pid - 1] */
static std::deque<int> ready_queue, waiting_queue;
static int proc_owner[NP];                  /* pid running on processor p, 0 = free */
static char freePages[NUM_PHYSICAL_PAGES];  /* 1 = allocated; frame 0 holds the page tables */
static int page_faults_on[NP];
static bool shell_open = true;
static std::string shell_buffer;

/* EXT e13: frame table, one entry per physical frame: which page of which task is in it
 * and since when. The page tables answer "page -> frame"; choosing a victim needs the
 * other direction, "frame -> page", plus the ages used by FIFO and LRU. */
struct FrameInfo {
    int pid = 0;                            /* owner; 0 = free (or frame 0) */
    int index = 0;                          /* logical page index in the owner's page table */
    long loaded_at = 0;                     /* when the page was brought into this frame (FIFO) */
    long last_used = 0;                     /* last access to it (LRU) */
};
static FrameInfo frame_info[NUM_PHYSICAL_PAGES];
static long mmu_clock = 0;                  /* EXT e13: ticks on every page placement and access */

/* EXT e13: backing store ("swap disk"): one PAGESIZE slot per task (pid) and data page.
 * swapped[pid-1][d] = data page d is not in memory but its contents are in the slot
 * (it was paged out, or it belongs to the data file and was never loaded yet). */
static unsigned char backing_store[MAX_TASKS][NUM_DATA_PAGES][PAGESIZE];
static bool swapped[MAX_TASKS][NUM_DATA_PAGES];

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

/* EXT e13: put logical page index of the task on processor p into frame (page table + frame table). */
static void map_page(int p, int index, int frame) {
    memory[PTBR[p] + index] = (unsigned char)frame;
    frame_info[frame].pid = proc_owner[p];
    frame_info[frame].index = index;
    frame_info[frame].loaded_at = frame_info[frame].last_used = ++mmu_clock;
}

/* EXT e13: page-out: save the data page in frame f to its owner's backing-store slot and
 * clear the owner's page-table entry, so the owner's next access to it page-faults.
 * The frame stays allocated: the caller reuses it. */
static void page_out(int f) {
    FrameInfo &fi = frame_info[f];
    Task &owner = tasks[fi.pid - 1];
    int d = fi.index - NUM_INSTR_PAGES;
    std::memcpy(backing_store[fi.pid - 1][d], &memory[f * PAGESIZE], PAGESIZE);
    swapped[fi.pid - 1][d] = true;
    memory[PTBR[owner.proc] + fi.index] = 0;               /* owner's page table (global: other processor) */
    owner.page_outs++;
    if (trace) std::fprintf(stderr, "[mmu] pid %d: data page %d paged out of frame %d\n", fi.pid, d, f);
    fi.pid = 0;
}

/* EXT e13: no free frame: choose a victim DATA page (instruction pages are never replaced),
 * FIFO = brought in earliest, LRU = used least recently. Local replacement looks only at
 * the faulting task's own pages; global at every task's. Returns the freed frame or -1. */
static int replace_page(int pid) {
    int victim = -1;
    long victim_age = 0;
    for (int f = 1; f < NUM_PHYSICAL_PAGES; f++) {
        const FrameInfo &fi = frame_info[f];
        if (fi.pid == 0 || fi.index < NUM_INSTR_PAGES) continue;      /* free, or a code page */
        if (!GLOBAL_REPLACEMENT && fi.pid != pid) continue;            /* local: own pages only */
        long age = REPLACE_LRU ? fi.last_used : fi.loaded_at;          /* smaller = older */
        if (victim < 0 || age < victim_age) { victim = f; victim_age = age; }
    }
    if (victim >= 0) page_out(victim);
    return victim;
}

/* EXT e13: page-fault handler for data page index of the task on processor p:
 * get a frame (free one, else replace a page), then fill it from the backing store
 * (page-in) or with zeros (first touch of a page that is not in the data file). */
static int page_fault(int p, int index) {
    int pid = proc_owner[p];
    int d = index - NUM_INSTR_PAGES;
    int frame = getFreePage();
    if (frame < 0) frame = replace_page(pid);
    if (frame < 0) {
        processor_error("ERROR: no free physical frame and no page to replace for data page %d", d);
        return -1;
    }
    bool page_in = swapped[pid - 1][d];
    if (page_in) {                                         /* page-in from the backing store */
        std::memcpy(&memory[frame * PAGESIZE], backing_store[pid - 1][d], PAGESIZE);
        swapped[pid - 1][d] = false;
        tasks[pid - 1].page_ins++;
    } else {
        std::memset(&memory[frame * PAGESIZE], 0, PAGESIZE);    /* zero-fill, as in Lab 5 */
    }
    if (trace) std::fprintf(stderr, "[mmu] pid %d: data page %d -> frame %d (%s)\n",
                            pid, d, frame, page_in ? "page-in" : "zero-fill");
    map_page(p, index, frame);
    page_faults_on[p]++;
    return frame;
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
        frame = page_fault(p, index);       /* EXT e13: free frame or replacement; page-in or zero-fill */
        if (frame < 0) return -1;
    }
    frame_info[frame].last_used = ++mmu_clock;     /* EXT e13: every access refreshes the page (LRU) */
    return frame * PAGESIZE + address % PAGESIZE;
}

/* Put bytes into freshly allocated frames, as logical pages first_index, first_index+1, ... */
static void copy_in(int p, int first_index, const std::vector<unsigned char> &bytes) {
    for (int pg = 0; pg < pages_for(bytes.size()); pg++) {
        int frame = getFreePage();          /* the loader checked that enough are free */
        map_page(p, first_index + pg, frame);              /* EXT e13: page table + frame table */
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
    /* EXT e13: only data page 0 gets a frame now, so the task always owns at least one data
     * frame (local replacement then always has a victim). Pages 1.. of the data image go
     * to the backing store and are paged in on first use. */
    std::vector<unsigned char> page0(t.data.begin(), t.data.begin() + (long)std::min(t.data.size(), (size_t)PAGESIZE));
    page0.resize(PAGESIZE, 0);                             /* a whole page, even for a tiny data file */
    copy_in(p, NUM_INSTR_PAGES, page0);
    for (int d = 1; d < pages_for(t.data.size()); d++) {
        size_t start = (size_t)d * PAGESIZE;
        size_t n = std::min((size_t)PAGESIZE, t.data.size() - start);
        std::memset(backing_store[t.pid - 1][d], 0, PAGESIZE);
        std::memcpy(backing_store[t.pid - 1][d], &t.data[start], n);
        swapped[t.pid - 1][d] = true;
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
        int d = (int)(a / PAGESIZE);
        int frame = memory[PTBR[p] + NUM_INSTR_PAGES + d];
        if (frame) out[a] = memory[frame * PAGESIZE + a % PAGESIZE];
        else if (swapped[t.pid - 1][d]) out[a] = backing_store[t.pid - 1][d][a % PAGESIZE];  /* EXT e13 */
    }
    std::string out_name = file_stem(t.data_path) + "_" + std::to_string(t.pid) + ".out";
    write_byte_file(out_name, out);
    std::string freq_name = write_frequency_file(p, t);

    int data_frames = 0;                                   /* EXT e13: resident data pages at the end */
    for (int i = 0; i < NUM_LOGICAL_PAGES; i++) {          /* release frames, clear the table */
        int frame = memory[PTBR[p] + i];
        if (frame) freePages[frame] = 0;
        if (frame) frame_info[frame] = FrameInfo();        /* EXT e13: frame table entry free again */
        if (frame && i >= NUM_INSTR_PAGES) data_frames++;
        memory[PTBR[p] + i] = 0;
    }
    for (int d = 0; d < NUM_DATA_PAGES; d++) swapped[t.pid - 1][d] = false;   /* EXT e13: free its slots */
    PTBR[p] = -1;

    t.steps = steps[p];
    t.page_faults = page_faults_on[p];
    t.failed = failed[p];
    std::printf("[os] pid %d (%s) %s on processor %d after %ld instructions, %d page fault(s) -> %s, %s\n",
                t.pid, t.prog.name.c_str(), t.failed ? "STOPPED WITH AN ERROR" : "finished", p,
                t.steps, t.page_faults, out_name.c_str(), freq_name.c_str());
    std::printf("[os] pid %d paging (%s %s): %d faults = %d zero-fill + %d page-in, %d page-out, "
                "%d data frame(s) at the end\n", t.pid, GLOBAL_REPLACEMENT ? "global" : "local",
                REPLACE_LRU ? "LRU" : "FIFO", t.page_faults, t.page_faults - t.page_ins, t.page_ins,
                t.page_outs, data_frames);                 /* EXT e13 */
}

/* ---------------- loader and tasks ---------------- */

/* loader(): start a waiting task if a processor and enough frames are free. */
static bool loader(int pid) {
    Task &t = tasks[pid - 1];
    int p = -1;
    for (int i = 0; i < NP; i++)
        if (!proc_owner[i]) { p = i; break; }
    int need = pages_for(t.prog.code.size()) + 1;   /* EXT e13: instruction pages + 1 data page, not all pages */
    if (p < 0 || need > free_page_count()) return false;

    proc_owner[p] = pid;
    t.proc = p;
    reset(p);
    current_pid[p] = pid;
    initialize(p, t);
    t.state = READY;
    ready_queue.push_back(pid);
    std::printf("[os] pid %d (%s) loaded on processor %d: %d instruction + 1 data page(s), %d more in the "
                "backing store, %d frame(s) left\n", pid, t.prog.name.c_str(), p, pages_for(t.prog.code.size()),
                std::max(0, pages_for(t.data.size()) - 1), free_page_count());     /* EXT e13 */
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
    if (ok && pages_for(t.prog.code.size()) + 1 > NUM_PHYSICAL_PAGES - 1) { err = "task needs more frames than physical memory has"; ok = false; }   /* EXT e13: code + 1 data page */
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
