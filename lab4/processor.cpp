/* Processor (handout "Processor specification"): executes the byte code of the task on
 * processor proc_id, using that processor's registers and memory (Instruction[proc_id],
 * Data[proc_id]). Lab 4 changes are marked: [NP] state, print, process_instructions().
 */
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include "memory.h"
#include "processor.h"

/* Lab 4: every piece of processor state is an array of NP, one entry per processor */
int Register[NP][NUM_XREGS];
int VRegister[NP][NUM_VREGS][VLEN];
int PC[NP];
int FlagN[NP], FlagZ[NP], FlagC[NP], FlagV[NP];
int end_of_simulation[NP];
int failed[NP];
int current_pid[NP];
long steps[NP];

int proc_id, opcode, dest, src1, src2;
FILE *fd_log = nullptr;

static int instr_pc;     /* address of the instruction being executed (PC already points past it) */

/* Report a run-time error and stop the task on this processor; the other tasks keep running. */
static void processor_error(const char *fmt, ...) {
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

/* reset(): registers, vector registers, flags and PC of processor p to 0;
 * Lab 4: also open the log file, in append mode. */
void reset(int p) {
    std::memset(Register[p], 0, sizeof Register[p]);
    std::memset(VRegister[p], 0, sizeof VRegister[p]);
    PC[p] = 0;
    FlagN[p] = FlagZ[p] = FlagC[p] = FlagV[p] = 0;
    end_of_simulation[p] = failed[p] = 0;
    steps[p] = 0;
    if (!fd_log && !(fd_log = std::fopen(LOG_FILE, "a")))
        std::fprintf(stderr, "cannot open %s: print output is lost\n", LOG_FILE);
}

/* ---- data memory of this processor: 32-bit little-endian words, any address 0 .. DATA_SIZE-4 ---- */

static bool load_word(int addr, int &value) {
    if (addr < 0 || addr > DATA_SIZE - 4) {
        processor_error("word at data address %d is outside Data[0..%d]", addr, DATA_SIZE - 1);
        return false;
    }
    const unsigned char *mem = Data[proc_id];
    unsigned int v = 0;
    for (int k = 0; k < 4; k++) v |= (unsigned int)mem[addr + k] << (8 * k);
    value = (int)v;
    return true;
}

static bool store_word(int addr, int value) {
    if (addr < 0 || addr > DATA_SIZE - 4) {
        processor_error("word at data address %d is outside Data[0..%d]", addr, DATA_SIZE - 1);
        return false;
    }
    unsigned char *mem = Data[proc_id];
    unsigned int v = (unsigned int)value;
    for (int k = 0; k < 4; k++) mem[addr + k] = (unsigned char)(v >> (8 * k));
    if (addr + 4 > data_size[proc_id]) data_size[proc_id] = addr + 4;   /* finalize() writes up to here */
    return true;
}

/* ---- Arithmetic operations: every addition/subtraction updates the flags Z N C V ---- */

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

/* Control operations: B{Suffix} is taken when its ARM condition holds */
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

/* fetch(): read the 4 instruction bytes at PC[proc_id] into opcode, dest, src1, src2. */
void fetch() {
    int p = proc_id;
    instr_pc = PC[p];
    if (instr_pc < 0 || instr_pc > INSTR_SIZE - 4) {
        processor_error("PC %d is outside Instruction[0..%d]", instr_pc, INSTR_SIZE - 1);
        return;
    }
    const unsigned char *ins = &Instruction[p][instr_pc];
    opcode = ins[0]; dest = ins[1]; src1 = ins[2]; src2 = ins[3];
    PC[p] = instr_pc + 4;
}

/* decode(): nothing to do; the fields are used directly by execute(). */
void decode() {}

/* ---- Vector arithmetic (Lab 3): + - *, 8 lanes of 32 bits ---- */

static bool bad_vreg(int r) {
    if (r < NUM_VREGS) return false;
    processor_error("vector register v%d does not exist", r);
    return true;
}

/* vD = vA op vB (21-23), vA op c (29-2B) or vA op xB (31-33): lane i = vA[i] op b[i]. Flags unchanged. */
static void vector_op() {
    int p = proc_id;
    int base = opcode >= OP_VADDX ? OP_VADDX : opcode >= OP_VADDI ? OP_VADDI : OP_VADD;
    int kind = opcode - base;                            /* 0 add, 1 sub, 2 mul */
    if (bad_vreg(dest) || bad_vreg(src1) || (base == OP_VADD && bad_vreg(src2))) return;
    for (int i = 0; i < VLEN; i++) {
        uint32_t a = (uint32_t)VRegister[p][src1][i];
        uint32_t b = base == OP_VADD  ? (uint32_t)VRegister[p][src2][i]    /* vector */
                   : base == OP_VADDI ? (uint32_t)src2                     /* constant 0..255 */
                                      : (uint32_t)Register[p][src2];       /* integer register */
        uint32_t r = kind == 0 ? a + b : kind == 1 ? a - b : a * b;
        VRegister[p][dest][i] = (int)r;
    }
}

/* execute(): perform the instruction; opcode 0 ends the task. */
void execute() {
    int p = proc_id;
    int *x = Register[p];
    steps[p]++;
    uint32_t a = (uint32_t)x[src1], b = 0;
    int value = 0, addr = 0;
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
    case OP_MOV:  x[dest] = x[src2]; break;                                    /* xD = xA */
    case OP_MOVC: x[dest] = src2; break;                                       /* xD = c  */
    case OP_LOAD:   if (load_word(x[src2], value)) x[dest] = value; break;     /* xD = [xA] */
    case OP_LOADC:  if (load_word(src2, value)) x[dest] = value; break;        /* xD = [c]  */
    case OP_STORE:  store_word(x[dest], x[src2]); break;                       /* [xA] = xS */
    case OP_STOREC: store_word(dest, x[src2]); break;                          /* [c] = xS  */
    case OP_PRINT:                                       /* Lab 4: Print operation -> log file */
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
    case OP_VLOAD: case OP_VLOADC: {                     /* vD = [xA] or [c]: lane i = word at address + 4i */
        if (bad_vreg(dest)) break;
        addr = opcode == OP_VLOAD ? x[src2] : src2;
        int lanes[VLEN] = {};
        for (int i = 0; i < VLEN; i++)
            if (!load_word(addr + 4 * i, lanes[i])) return;
        std::memcpy(VRegister[p][dest], lanes, sizeof lanes);
        break;
    }
    case OP_VSTORE: case OP_VSTOREC:                     /* [xA] or [c] = vS: word at address + 4i = lane i */
        if (bad_vreg(src2)) break;
        addr = opcode == OP_VSTORE ? x[dest] : dest;
        for (int i = 0; i < VLEN; i++)
            if (!store_word(addr + 4 * i, VRegister[p][src2][i])) return;
        break;
    default:
        if (opcode >= OP_BRANCH && opcode < OP_BRANCH + NUM_CONDITIONS) {   /* B<cond> .label */
            if (condition_holds(p, opcode - OP_BRANCH))
                PC[p] = instr_pc + 4 * (signed char)src2;    /* offset counts instructions from this one */
        } else {
            processor_error("unknown opcode 0x%02X", opcode);
        }
    }
    if (!end_of_simulation[p] && steps[p] >= MAX_STEPS)
        processor_error("%ld instructions executed without reaching the end: endless loop?", MAX_STEPS);
}

/* Lab 4: process_instructions(proc_id, count) - run up to count instructions of the task on
 * processor p (fewer if it ends), then sleep 10 us so the execution can be watched. */
int process_instructions(int p, int count) {
    proc_id = p;
    int done = 0;
    for (int i = 0; i < count && !end_of_simulation[p]; i++) {
        fetch();
        if (end_of_simulation[p]) break;                 /* PC outside Instruction[p] */
        decode();
        execute();
        done++;
    }
    usleep(SLEEP_US);
    return done;
}
