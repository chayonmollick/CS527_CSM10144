/* Processor: executes byte code for the task mapped to processor proc_id.
 * Every memory access goes through getPhysicalAddress() (Lab 5 MMU, in os.cpp).
 * EXT e12: data loads and stores then go through this processor's data cache.
 */
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include "compiler.h"
#include "memory.h"
#include "os.h"
#include "processor.h"

int Register[NP][NUM_XREGS];
int VRegister[NP][NUM_VREGS][VLEN];
int PC[NP];
int FlagN[NP], FlagZ[NP], FlagC[NP], FlagV[NP];
int end_of_simulation[NP];
int failed[NP];
int PTBR[NP];                    /* set to -1 (no page table) by os_init() */
int current_pid[NP];
long steps[NP];
long exec_count[NP][MAX_INSTRUCTIONS];
int data_high[NP];

int proc_id, opcode, dest, src1, src2;
FILE *fd_log = nullptr;
int trace = 0;

static int instr_pc;     /* address of the instruction being executed (PC already points past it) */

/* ---- EXT e12: data cache, one per processor, indexed by PHYSICAL address ----
 * Direct-mapped: physical address pa lives in line (pa / LINE_SIZE) % CACHE_LINES with
 * tag pa / (LINE_SIZE * CACHE_LINES); the tag tells which of the many memory lines that
 * share this cache line is there now. A line keeps its own copy of LINE_SIZE bytes, so
 * with write-back the newest data can be in the cache and not yet in memory[].
 * Instruction fetches and page-table reads (the MMU) go straight to memory[]. */
struct CacheLine {
    bool valid = false;
    bool dirty = false;                    /* write-back only: the line is newer than memory */
    int tag = 0;
    unsigned char data[LINE_SIZE] = {};
};
static CacheLine dcache[NP][CACHE_LINES];
long cache_hits[NP], cache_misses[NP], cache_writebacks[NP], memory_bytes_written[NP];

static int line_index(int pa) { return (pa / LINE_SIZE) % CACHE_LINES; }
static int line_tag(int pa)   { return pa / (LINE_SIZE * CACHE_LINES); }

static bool in_cache(int p, int pa) {
    const CacheLine &line = dcache[p][line_index(pa)];
    return line.valid && line.tag == line_tag(pa);
}

static void write_back(int p, int i) {     /* copy dirty line i of processor p to memory */
    CacheLine &line = dcache[p][i];
    int base = (line.tag * CACHE_LINES + i) * LINE_SIZE;   /* rebuild its physical address */
    std::memcpy(&memory[base], line.data, LINE_SIZE);
    line.dirty = false;
    cache_writebacks[p]++;
    memory_bytes_written[p] += LINE_SIZE;
}

static void fill_line(int p, int pa) {     /* miss: load the line holding pa (evict the old one) */
    int i = line_index(pa);
    CacheLine &line = dcache[p][i];
    if (line.valid && line.dirty) write_back(p, i);       /* only write-back lines are dirty */
    std::memcpy(line.data, &memory[pa - pa % LINE_SIZE], LINE_SIZE);
    line.valid = true;
    line.dirty = false;
    line.tag = line_tag(pa);
}

/* Read one byte through the cache. hit is cleared if the byte was not in the cache. */
static unsigned char cache_read(int p, int pa, bool &hit) {
    if (!in_cache(p, pa)) { hit = false; fill_line(p, pa); }
    return dcache[p][line_index(pa)].data[pa % LINE_SIZE];
}

/* Write one byte through the cache, with the policy chosen by WRITE_BACK. */
static void cache_write(int p, int pa, unsigned char value, bool &hit) {
    bool present = in_cache(p, pa);
    if (!present) hit = false;
#if WRITE_BACK
    if (!present) fill_line(p, pa);                         /* write-allocate */
    CacheLine &line = dcache[p][line_index(pa)];
    line.data[pa % LINE_SIZE] = value;
    line.dirty = true;                                      /* memory is updated on eviction or flush */
#else
    if (present) dcache[p][line_index(pa)].data[pa % LINE_SIZE] = value;   /* keep the copy current */
    memory[pa] = value;                                     /* write-through; a miss does not allocate */
    memory_bytes_written[p]++;
#endif
}

/* End of task: write every dirty line back so memory[] (read by finalize) is up to date,
 * then invalidate, because the task's frames are freed and will be reused by other tasks. */
int cache_flush(int p) {
    int written = 0;
    for (int i = 0; i < CACHE_LINES; i++) {
        if (dcache[p][i].valid && dcache[p][i].dirty) { write_back(p, i); written++; }
        dcache[p][i].valid = false;
    }
    return written;
}

void processor_error(const char *fmt, ...) {
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[pid %d, processor %d] error at instruction %d: %s\n",
                 current_pid[proc_id], proc_id, instr_pc / 4, msg);
    failed[proc_id] = 1;
    end_of_simulation[proc_id] = 1;
}

/* reset(): clear the registers, PC and flags of processor p; open the log file (append). */
void reset(int p) {
    std::memset(Register[p], 0, sizeof Register[p]);
    std::memset(VRegister[p], 0, sizeof VRegister[p]);
    std::memset(exec_count[p], 0, sizeof exec_count[p]);
    PC[p] = 0;
    FlagN[p] = FlagZ[p] = FlagC[p] = FlagV[p] = 0;
    end_of_simulation[p] = failed[p] = 0;
    steps[p] = 0;
    data_high[p] = 0;
    for (int i = 0; i < CACHE_LINES; i++) dcache[p][i].valid = false;              /* EXT e12: empty cache */
    cache_hits[p] = cache_misses[p] = cache_writebacks[p] = memory_bytes_written[p] = 0;
    if (!fd_log) fd_log = std::fopen(LOG_FILE, "a");
}

/* ---- logical memory access (one byte at a time, so a word may cross a page boundary) ---- */

static bool read_byte(int is_fetch, int addr, unsigned char &value) {
    int pa = getPhysicalAddress(proc_id, is_fetch, addr);
    if (pa < 0) return false;                    /* fault already reported */
    value = memory[pa];
    return true;
}

/* EXT e12: loads and stores go through the data cache (fetches still use read_byte).
 * Statistics count one access per word: it is a hit only if all 4 bytes were cached. */
static bool load_word(int addr, int &value) {   /* 32-bit little-endian */
    unsigned int v = 0;
    bool hit = true;                                       /* EXT e12 */
    for (int k = 3; k >= 0; k--) {
        int pa = getPhysicalAddress(proc_id, 0, addr + k);
        if (pa < 0) return false;                          /* fault already reported */
        v = (v << 8) | cache_read(proc_id, pa, hit);       /* EXT e12: through the cache */
    }
    if (hit) cache_hits[proc_id]++; else cache_misses[proc_id]++;   /* EXT e12 */
    value = (int)v;
    return true;
}

static bool store_word(int addr, int value) {
    unsigned int v = (unsigned int)value;
    bool hit = true;                                       /* EXT e12 */
    for (int k = 0; k < 4; k++) {
        int pa = getPhysicalAddress(proc_id, 0, addr + k);
        if (pa < 0) return false;
        cache_write(proc_id, pa, (unsigned char)(v >> (8 * k)), hit);   /* EXT e12: through the cache */
    }
    if (hit) cache_hits[proc_id]++; else cache_misses[proc_id]++;   /* EXT e12 */
    if (addr + 4 > data_high[proc_id]) data_high[proc_id] = addr + 4;
    return true;
}

/* ---- flags (Lab 2): only add and subtract change them ---- */

static void set_add_flags(int p, uint32_t a, uint32_t b, uint32_t r) {
    FlagZ[p] = (r == 0);
    FlagN[p] = (int)(r >> 31);
    FlagC[p] = (r < a) || (r < b);                       /* unsigned result smaller than an input */
    FlagV[p] = ((a >> 31) == (b >> 31)) && ((r >> 31) != (a >> 31));   /* same signs in, other sign out */
}

static void set_sub_flags(int p, uint32_t a, uint32_t b, uint32_t r) {
    FlagZ[p] = (r == 0);
    FlagN[p] = (int)(r >> 31);
    FlagC[p] = (a > b);                                  /* operand 1 bigger than operand 2 (unsigned) */
    FlagV[p] = ((a >> 31) != (b >> 31)) && ((r >> 31) == (b >> 31));   /* signs differ, result has b's sign */
}

static bool condition_holds(int p, int cond) {
    int N = FlagN[p], Z = FlagZ[p], C = FlagC[p], V = FlagV[p];
    switch (cond) {
    case COND_EQ: return Z;            case COND_NE: return !Z;
    case COND_CS: return C;            case COND_CC: return !C;
    case COND_MI: return N;            case COND_PL: return !N;
    case COND_VS: return V;            case COND_VC: return !V;
    case COND_HI: return C && !Z;      case COND_LS: return !C || Z;
    case COND_GE: return N == V;       case COND_LT: return N != V;
    case COND_GT: return !Z && N == V; case COND_LE: return Z || N != V;
    default:      return true;         /* AL */
    }
}

/* fetch(): read the 4 instruction bytes at PC into opcode, dest, src1, src2. */
void fetch() {
    int p = proc_id;
    instr_pc = PC[p];
    unsigned char b[4];
    for (int k = 0; k < 4; k++)
        if (!read_byte(1, instr_pc + k, b[k])) return;
    opcode = b[0]; dest = b[1]; src1 = b[2]; src2 = b[3];
    PC[p] = instr_pc + 4;
}

/* decode(): nothing to do; the fields are used directly by execute(). */
void decode() {}

static bool bad_vreg(int r) {
    if (r < NUM_VREGS) return false;
    processor_error("vector register v%d does not exist", r);
    return true;
}

static void vector_op() {
    int p = proc_id;
    int base = opcode >= OP_VADDX ? OP_VADDX : opcode >= OP_VADDI ? OP_VADDI : OP_VADD;
    int kind = opcode - base;                            /* 0 add, 1 sub, 2 mul */
    if (bad_vreg(dest) || bad_vreg(src1) || (base == OP_VADD && bad_vreg(src2))) return;
    for (int i = 0; i < VLEN; i++) {
        uint32_t a = (uint32_t)VRegister[p][src1][i];
        uint32_t b = base == OP_VADD  ? (uint32_t)VRegister[p][src2][i]
                   : base == OP_VADDI ? (uint32_t)src2
                                      : (uint32_t)Register[p][src2];
        uint32_t r = kind == 0 ? a + b : kind == 1 ? a - b : a * b;
        VRegister[p][dest][i] = (int)r;
    }
}

/* execute(): perform the instruction; opcode 0 ends the task. */
void execute() {
    int p = proc_id;
    int *x = Register[p];
    exec_count[p][instr_pc / 4]++;                       /* Lab 6 task 1 */
    steps[p]++;
    if (trace)
        std::fprintf(stderr, "[pid %d p%d] %3d: %02X %02X %02X %02X   %s\n", current_pid[p], p,
                     instr_pc / 4, opcode, dest, src1, src2, disassemble(opcode, dest, src1, src2).c_str());

    uint32_t a = (uint32_t)x[src1], b;
    int value, addr;
    switch (opcode) {
    case OP_HALT:
        end_of_simulation[p] = 1;
        break;
    case OP_ADD: case OP_ADDI:
        b = opcode == OP_ADD ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a + b);
        set_add_flags(p, a, b, a + b);
        break;
    case OP_SUB: case OP_SUBI:
        b = opcode == OP_SUB ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a - b);
        set_sub_flags(p, a, b, a - b);
        break;
    case OP_MUL: case OP_MULI:
        b = opcode == OP_MUL ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a * b);
        break;
    case OP_DIV: case OP_DIVI: {
        int divisor = opcode == OP_DIV ? x[src2] : src2;
        if (divisor == 0) { processor_error("division by zero"); break; }
        x[dest] = (x[src1] == INT_MIN && divisor == -1) ? INT_MIN : x[src1] / divisor;
        break;
    }
    case OP_MOV:  x[dest] = x[src2]; break;
    case OP_MOVC: x[dest] = src2; break;
    case OP_LOAD:   if (load_word(x[src2], value)) x[dest] = value; break;     /* xD = [xA] */
    case OP_LOADC:  if (load_word(src2, value)) x[dest] = value; break;        /* xD = [c]  */
    case OP_STORE:  store_word(x[dest], x[src2]); break;                       /* [xA] = xS */
    case OP_STOREC: store_word(dest, x[src2]); break;                          /* [c] = xS  */
    case OP_PRINT:
        if (fd_log) {
            std::fprintf(fd_log, "Process id: %d   x%d : %08X\n", current_pid[p], src2, (unsigned)x[src2]);
            std::fflush(fd_log);
        }
        break;
    case OP_VADD: case OP_VSUB: case OP_VMUL:
    case OP_VADDI: case OP_VSUBI: case OP_VMULI:
    case OP_VADDX: case OP_VSUBX: case OP_VMULX:
        vector_op();
        break;
    case OP_VLOAD: case OP_VLOADC: {                     /* vD = [xA] or [c]: 8 consecutive words */
        if (bad_vreg(dest)) break;
        addr = opcode == OP_VLOAD ? x[src2] : src2;
        int lanes[VLEN];
        for (int i = 0; i < VLEN; i++)
            if (!load_word(addr + 4 * i, lanes[i])) return;
        std::memcpy(VRegister[p][dest], lanes, sizeof lanes);
        break;
    }
    case OP_VSTORE: case OP_VSTOREC:                     /* [xA] or [c] = vS */
        if (bad_vreg(src2)) break;
        addr = opcode == OP_VSTORE ? x[dest] : dest;
        for (int i = 0; i < VLEN; i++)
            if (!store_word(addr + 4 * i, VRegister[p][src2][i])) return;
        break;
    default:
        if (opcode >= OP_BRANCH && opcode < OP_BRANCH + NUM_CONDITIONS) {   /* Lab 2 */
            if (condition_holds(p, opcode - OP_BRANCH))
                PC[p] = instr_pc + 4 * (signed char)src2;    /* offset counts instructions */
        } else {
            processor_error("unknown opcode 0x%02X", opcode);
        }
    }
}

/* Lab 4: run up to count instructions of the task on processor p, then pause. */
int process_instructions(int p, int count) {
    proc_id = p;
    int done = 0;
    for (int i = 0; i < count && !end_of_simulation[p]; i++) {
        fetch();
        if (end_of_simulation[p]) break;                 /* fetch fault */
        decode();
        execute();
        done++;
        if (!end_of_simulation[p] && steps[p] >= MAX_STEPS) {
            processor_error("%ld instructions executed without halting: endless loop?", MAX_STEPS);
            break;
        }
    }
    usleep(SLEEP_US);
    return done;
}
