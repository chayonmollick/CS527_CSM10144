/* Processor (handout "Processor specification"): executes the byte code in Instruction[]
 * on the data in Data[]. The Lab 3 additions (vector registers and instructions) are marked.
 */
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "memory.h"
#include "processor.h"

int Register[NUM_XREGS];
int VRegister[NUM_VREGS][VLEN];
int PC, opcode, dest, src1, src2;
int FlagN, FlagZ, FlagC, FlagV;
int end_of_simulation = 0;
int failed = 0;
long steps = 0;

static int instr_pc;     /* address of the instruction being executed (PC already points past it) */

/* Report a run-time error and stop the program. */
static void processor_error(const char *fmt, ...) {
    char msg[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "error at instruction %d: %s\n", instr_pc / 4, msg);
    failed = 1;
    end_of_simulation = 1;
}

/* reset(): every register (x and v) and flag to 0, PC to 0. */
void reset() {
    std::memset(Register, 0, sizeof Register);
    std::memset(VRegister, 0, sizeof VRegister);
    PC = 0;
    FlagN = FlagZ = FlagC = FlagV = 0;
    end_of_simulation = failed = 0;
    steps = 0;
}

/* ---- data memory: 32-bit little-endian words, any byte address 0 .. DATA_SIZE-4 ---- */

static bool load_word(int addr, int &value) {
    if (addr < 0 || addr > DATA_SIZE - 4) {
        processor_error("word at data address %d is outside Data[0..%d]", addr, DATA_SIZE - 1);
        return false;
    }
    unsigned int v = 0;
    for (int k = 0; k < 4; k++) v |= (unsigned int)Data[addr + k] << (8 * k);
    value = (int)v;
    return true;
}

static bool store_word(int addr, int value) {
    if (addr < 0 || addr > DATA_SIZE - 4) {
        processor_error("word at data address %d is outside Data[0..%d]", addr, DATA_SIZE - 1);
        return false;
    }
    unsigned int v = (unsigned int)value;
    for (int k = 0; k < 4; k++) Data[addr + k] = (unsigned char)(v >> (8 * k));
    if (addr + 4 > data_size) data_size = addr + 4;    /* finalize() writes up to here */
    return true;
}

/* ---- Arithmetic operations: every addition/subtraction updates the flags Z N C V ---- */

static void set_add_flags(uint32_t a, uint32_t b, uint32_t r) {
    FlagZ = (r == 0);
    FlagN = (int)(r >> 31);
    FlagC = (r < a) || (r < b);                          /* unsigned result smaller than an input */
    FlagV = ((a >> 31) == (b >> 31)) && ((r >> 31) != (a >> 31));   /* same signs in, other sign out */
}

static void set_sub_flags(uint32_t a, uint32_t b, uint32_t r) {
    FlagZ = (r == 0);
    FlagN = (int)(r >> 31);
    FlagC = (a > b);                                     /* operand 1 bigger than operand 2 (unsigned) */
    FlagV = ((a >> 31) != (b >> 31)) && ((r >> 31) == (b >> 31));   /* signs differ, result has b's sign */
}

/* Control operations: B{Suffix} is taken when its ARM condition holds */
static bool condition_holds(int cond) {
    int N = FlagN, Z = FlagZ, C = FlagC, V = FlagV;
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
    instr_pc = PC;
    if (PC < 0 || PC > INSTR_SIZE - 4) {
        processor_error("PC %d is outside Instruction[0..%d]", PC, INSTR_SIZE - 1);
        return;
    }
    opcode = Instruction[PC];
    dest   = Instruction[PC + 1];
    src1   = Instruction[PC + 2];
    src2   = Instruction[PC + 3];
    PC += 4;
}

/* decode(): nothing to do; the fields are used directly by execute(). */
void decode() {}

/* ---- Lab 3: Vector arithmetic (+ - *, 8 lanes of 32 bits) ---- */

static bool bad_vreg(int r) {
    if (r < NUM_VREGS) return false;
    processor_error("vector register v%d does not exist", r);
    return true;
}

/* vD = vA op vB (21-23), vA op c (29-2B) or vA op xB (31-33): lane i = vA[i] op b[i]. Flags unchanged. */
static void vector_op() {
    int base = opcode >= OP_VADDX ? OP_VADDX : opcode >= OP_VADDI ? OP_VADDI : OP_VADD;
    int kind = opcode - base;                            /* 0 add, 1 sub, 2 mul */
    if (bad_vreg(dest) || bad_vreg(src1) || (base == OP_VADD && bad_vreg(src2))) return;
    for (int i = 0; i < VLEN; i++) {
        uint32_t a = (uint32_t)VRegister[src1][i];
        uint32_t b = base == OP_VADD  ? (uint32_t)VRegister[src2][i]    /* vector */
                   : base == OP_VADDI ? (uint32_t)src2                  /* constant 0..255 */
                                      : (uint32_t)Register[src2];       /* integer register */
        uint32_t r = kind == 0 ? a + b : kind == 1 ? a - b : a * b;
        VRegister[dest][i] = (int)r;
    }
}

/* execute(): perform the instruction; opcode 0 ends the program. */
void execute() {
    int *x = Register;
    steps++;
    uint32_t a = (uint32_t)x[src1], b = 0;
    int value = 0, addr = 0;
    switch (opcode) {
    case OP_HALT:
        end_of_simulation = 1;
        break;
    case OP_ADD: case OP_ADDI:
        b = opcode == OP_ADD ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a + b);
        set_add_flags(a, b, a + b);
        break;
    case OP_SUB: case OP_SUBI:
        b = opcode == OP_SUB ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a - b);
        set_sub_flags(a, b, a - b);
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
    /* ---- Lab 3: vector instructions ---- */
    case OP_VADD: case OP_VSUB: case OP_VMUL:
    case OP_VADDI: case OP_VSUBI: case OP_VMULI:
    case OP_VADDX: case OP_VSUBX: case OP_VMULX:
        vector_op();
        break;
    case OP_VLOAD: case OP_VLOADC: {                     /* vD = [xA] / [c]: lane i = word at address + 4i */
        if (bad_vreg(dest)) break;
        addr = opcode == OP_VLOAD ? x[src2] : src2;
        int lanes[VLEN] = {};
        for (int i = 0; i < VLEN; i++)
            if (!load_word(addr + 4 * i, lanes[i])) return;
        std::memcpy(VRegister[dest], lanes, sizeof lanes);
        break;
    }
    case OP_VSTORE: case OP_VSTOREC:                     /* [xA] or [c] = vS: word at address + 4i = lane i */
        if (bad_vreg(src2)) break;
        addr = opcode == OP_VSTORE ? x[dest] : dest;
        for (int i = 0; i < VLEN; i++)
            if (!store_word(addr + 4 * i, VRegister[src2][i])) return;
        break;
    default:
        if (opcode >= OP_BRANCH && opcode < OP_BRANCH + NUM_CONDITIONS) {   /* B<cond> .label */
            if (condition_holds(opcode - OP_BRANCH))
                PC = instr_pc + 4 * (signed char)src2;   /* offset counts instructions from this one */
        } else {
            processor_error("unknown opcode 0x%02X", opcode);
        }
    }
    if (!end_of_simulation && steps >= MAX_STEPS)
        processor_error("%ld instructions executed without reaching the end: endless loop?", MAX_STEPS);
}
