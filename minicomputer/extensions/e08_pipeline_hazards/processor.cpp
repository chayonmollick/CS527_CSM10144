/* Processor: executes byte code for the task mapped to processor proc_id.
 * Every memory access goes through getPhysicalAddress() (Lab 5 MMU, in os.cpp).
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

/* EXT e08: pipeline model of each processor. Register numbers: x0..x255 = 0..255,
 * v0..v31 = 256..287. A "slot" is a pipeline position in the stall-free schedule:
 * +1 per instruction, +PIPE_BRANCH_PENALTY bubbles after a taken branch. */
#define PIPE_REGS (NUM_XREGS + NUM_VREGS)
PipeStats pipe_stats[NP];
std::vector<std::string> pipe_log[NP];
static long next_slot[NP];                       /* slot of the next instruction */
static long next_id_nofwd[NP], next_id_fwd[NP];  /* earliest ID cycle of the next instruction */
static long written_slot[NP][PIPE_REGS];         /* slot of the last writer of each register */
static bool written_by_load[NP][PIPE_REGS];
static long ready_nofwd[NP][PIPE_REGS];          /* first ID cycle that can read the new value */
static long ready_fwd[NP][PIPE_REGS];

int proc_id, opcode, dest, src1, src2;
FILE *fd_log = nullptr;
int trace = 0;

static int instr_pc;     /* address of the instruction being executed (PC already points past it) */

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
    /* EXT e08: empty pipeline; the first instruction is in IF in cycle 1 and in ID in cycle 2 */
    pipe_stats[p] = PipeStats();
    pipe_log[p].clear();
    next_slot[p] = next_id_nofwd[p] = next_id_fwd[p] = 2;
    for (int r = 0; r < PIPE_REGS; r++) {
        written_slot[p][r] = -100;                     /* never written: no hazard */
        written_by_load[p][r] = false;
        ready_nofwd[p][r] = ready_fwd[p][r] = 0;
    }
    if (!fd_log) fd_log = std::fopen(LOG_FILE, "a");
}

/* ---- logical memory access (one byte at a time, so a word may cross a page boundary) ---- */

static bool read_byte(int is_fetch, int addr, unsigned char &value) {
    int pa = getPhysicalAddress(proc_id, is_fetch, addr);
    if (pa < 0) return false;                    /* fault already reported */
    value = memory[pa];
    return true;
}

static bool load_word(int addr, int &value) {   /* 32-bit little-endian */
    unsigned int v = 0;
    for (int k = 0; k < 4; k++) {               /* lowest address first, so a fault names addr */
        unsigned char b;
        if (!read_byte(0, addr + k, b)) return false;
        v |= (unsigned int)b << (8 * k);
    }
    value = (int)v;
    return true;
}

static bool store_word(int addr, int value) {
    unsigned int v = (unsigned int)value;
    for (int k = 0; k < 4; k++) {
        int pa = getPhysicalAddress(proc_id, 0, addr + k);
        if (pa < 0) return false;
        memory[pa] = (unsigned char)(v >> (8 * k));
    }
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

/* EXT e08: registers read and written by an instruction (numbers as in PIPE_REGS).
 * Address registers of loads and stores are reads too. Returns how many it reads. */
static int pipe_operands(int op, int d, int a, int b, int reads[2], int &write, bool &is_load) {
    int n = 0;
    write = -1;
    is_load = false;
    auto read_x = [&](int r) { reads[n++] = r; };
    auto read_v = [&](int r) { if (r < NUM_VREGS) reads[n++] = NUM_XREGS + r; };   /* bad v: task stops */
    auto write_v = [&](int r) { if (r < NUM_VREGS) write = NUM_XREGS + r; };
    switch (op) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:     read_x(a); read_x(b); write = d; break;
    case OP_ADDI: case OP_SUBI: case OP_MULI: case OP_DIVI: read_x(a); write = d; break;
    case OP_MOV:     read_x(b); write = d; break;
    case OP_MOVC:    write = d; break;
    case OP_LOAD:    read_x(b); write = d; is_load = true; break;   /* xD = [xA]: xA is the address */
    case OP_LOADC:   write = d; is_load = true; break;
    case OP_STORE:   read_x(d); read_x(b); break;                   /* [xA] = xS: reads both */
    case OP_STOREC:  read_x(b); break;
    case OP_PRINT:   read_x(b); break;
    case OP_VADD: case OP_VSUB: case OP_VMUL:    read_v(a); read_v(b); write_v(d); break;
    case OP_VADDI: case OP_VSUBI: case OP_VMULI: read_v(a); write_v(d); break;
    case OP_VADDX: case OP_VSUBX: case OP_VMULX: read_v(a); read_x(b); write_v(d); break;
    case OP_VLOAD:   read_x(b); write_v(d); is_load = true; break;
    case OP_VLOADC:  write_v(d); is_load = true; break;
    case OP_VSTORE:  read_x(d); read_v(b); break;
    case OP_VSTOREC: read_v(b); break;
    default:         break;                                        /* halt, branches: no registers */
    }
    if (n == 2 && reads[0] == reads[1]) n = 1;                     /* x2 = x1 + x1 reads x1 once */
    return n;
}

static std::string reg_name(int r) {                            /* EXT e08 */
    return r < NUM_XREGS ? "x" + std::to_string(r) : "v" + std::to_string(r - NUM_XREGS);
}

/* EXT e08: put the instruction being executed through the two pipelines.
 *   hazard: it reads a register whose last writer is 1 or 2 slots earlier.
 *   no forwarding: it waits in ID until the writer's WB (writer's ID + 3).
 *   forwarding: only a value from a load arrives late (load's ID + 2): load-use stall.
 * taken = it is a branch that is taken: PIPE_BRANCH_PENALTY bubbles follow it. */
static void pipeline_step(int p, int taken) {
    PipeStats &s = pipe_stats[p];
    int reads[2], write;
    bool is_load;
    int n = pipe_operands(opcode, dest, src1, src2, reads, write, is_load);
    long slot = next_slot[p];
    long id_nofwd = next_id_nofwd[p], id_fwd = next_id_fwd[p];  /* ID cycle if nothing stalls it */
    std::string hazards;
    for (int k = 0; k < n; k++) {
        int r = reads[k];
        long distance = slot - written_slot[p][r];
        if (distance <= 2) {                                    /* RAW hazard */
            bool load_use = distance == 1 && written_by_load[p][r];
            if (distance == 1) s.raw_d1++; else s.raw_d2++;
            if (load_use) s.load_use++;
            hazards += reg_name(r) + ":d" + std::to_string(distance) + (load_use ? "(load) " : " ");
        }
        if (ready_nofwd[p][r] > id_nofwd) id_nofwd = ready_nofwd[p][r];
        if (ready_fwd[p][r] > id_fwd) id_fwd = ready_fwd[p][r];
    }
    long stall_nofwd = id_nofwd - next_id_nofwd[p], stall_fwd = id_fwd - next_id_fwd[p];
    s.instructions++;
    s.stalls_nofwd += stall_nofwd;
    s.stalls_fwd += stall_fwd;
    if (write >= 0) {
        written_slot[p][write] = slot;
        written_by_load[p][write] = is_load;
        ready_nofwd[p][write] = id_nofwd + PIPE_NOFWD_GAP;
        ready_fwd[p][write] = id_fwd + (is_load ? PIPE_LOAD_GAP : 1);
    }
    int bubbles = taken ? PIPE_BRANCH_PENALTY : 0;
    if (pipe_log[p].size() < PIPE_LOG_ROWS) {
        char row[160];
        std::snprintf(row, sizeof row, "%5ld %5d  %-18s %-22s %6ld %5ld %8ld %5ld", s.instructions, instr_pc / 4,
                      disassemble(opcode, dest, src1, src2).c_str(), hazards.c_str(), id_nofwd, stall_nofwd,
                      id_fwd, stall_fwd);
        pipe_log[p].push_back(row + (taken ? "   taken: " + std::to_string(bubbles) + " bubbles" : ""));
    }
    s.taken_branches += taken;
    next_slot[p] = slot + 1 + bubbles;
    next_id_nofwd[p] = id_nofwd + 1 + bubbles;
    next_id_fwd[p] = id_fwd + 1 + bubbles;
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

    /* EXT e08: pipeline model. A branch does not change the flags, so its outcome can be
     * evaluated here (the switch below then takes the same decision). */
    int is_branch = opcode >= OP_BRANCH && opcode < OP_BRANCH + NUM_CONDITIONS;
    pipeline_step(p, is_branch && condition_holds(p, opcode - OP_BRANCH));

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
