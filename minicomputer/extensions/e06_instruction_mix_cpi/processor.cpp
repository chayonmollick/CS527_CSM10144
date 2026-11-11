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

/* EXT e06: instruction mix of the task on each processor (cleared by reset()) */
long class_count[NP][NUM_CLASSES];
const char *CLASS_NAME[NUM_CLASSES] = {
    "alu", "mul", "div", "load", "store", "branch_taken", "branch_not_taken",
    "vector", "vector_mem", "print", "halt"};
const int CLASS_LATENCY[NUM_CLASSES] = {
    LAT_ALU, LAT_MUL, LAT_DIV, LAT_LOAD, LAT_STORE, LAT_BRANCH + LAT_TAKEN_PENALTY, LAT_BRANCH,
    LAT_VECTOR, LAT_VMEM, LAT_PRINT, LAT_HALT};

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
    std::memset(class_count[p], 0, sizeof class_count[p]);    /* EXT e06 */
    PC[p] = 0;
    FlagN[p] = FlagZ[p] = FlagC[p] = FlagV[p] = 0;
    end_of_simulation[p] = failed[p] = 0;
    steps[p] = 0;
    data_high[p] = 0;
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
    for (int k = 3; k >= 0; k--) {
        unsigned char b;
        if (!read_byte(0, addr + k, b)) return false;
        v = (v << 8) | b;
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

/* EXT e06: class of an instruction; taken = 1 if it is a branch whose condition holds. */
int instr_class(int op, int taken) {
    switch (op) {
    case OP_HALT:                                   return CLS_HALT;
    case OP_ADD: case OP_SUB: case OP_ADDI: case OP_SUBI:
    case OP_MOV: case OP_MOVC:                      return CLS_ALU;
    case OP_MUL: case OP_MULI:                      return CLS_MUL;
    case OP_DIV: case OP_DIVI:                      return CLS_DIV;
    case OP_LOAD: case OP_LOADC:                    return CLS_LOAD;
    case OP_STORE: case OP_STOREC:                  return CLS_STORE;
    case OP_PRINT:                                  return CLS_PRINT;
    case OP_VADD: case OP_VSUB: case OP_VMUL:
    case OP_VADDI: case OP_VSUBI: case OP_VMULI:
    case OP_VADDX: case OP_VSUBX: case OP_VMULX:    return CLS_VECTOR;
    case OP_VLOAD: case OP_VLOADC:
    case OP_VSTORE: case OP_VSTOREC:                return CLS_VMEM;
    default:
        if (op >= OP_BRANCH && op < OP_BRANCH + NUM_CONDITIONS)
            return taken ? CLS_BRANCH_TAKEN : CLS_BRANCH_NOT_TAKEN;
        return -1;                                  /* unknown opcode: the task stops anyway */
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

    /* EXT e06: count the instruction in its class. A branch does not change the flags,
     * so evaluating its condition here gives the same answer as the switch below. */
    int is_branch = opcode >= OP_BRANCH && opcode < OP_BRANCH + NUM_CONDITIONS;
    int cls = instr_class(opcode, is_branch && condition_holds(p, opcode - OP_BRANCH));
    if (cls >= 0) class_count[p][cls]++;

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
        if (steps[p] >= MAX_STEPS) {
            processor_error("more than %ld instructions executed: endless loop?", MAX_STEPS);
            break;
        }
    }
    usleep(SLEEP_US);
    return done;
}
