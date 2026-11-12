/* Processor (handout: Processor specification): the registers, PC, the flags N Z C V
 * and the reset / fetch / decode / execute cycle. "Assume all the registers in the
 * architecture are of 32 bits"; "Data size is always 32 bit".
 */
#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "memory.h"
#include "processor.h"

int Register[NUM_REGS];                 /* handout: int Register[256] */
int PC, opcode, dest, src1, src2;       /* handout: int PC, opcode, dest, src1, src2 */
int FlagN, FlagZ, FlagC, FlagV;         /* handout: global flags N Z C V */
int end_of_simulation = 0;              /* handout: int end_of_simulation = 0 */
int failed = 0;
long steps = 0;

static int instr_pc;    /* address of the instruction being executed (PC already points past it) */

/* A run-time error: report it and stop the simulation. */
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

/* reset(): "initializes each element of register array to 0 as well initialize PC to 0";
 * the flags start at 0 too. */
void reset() {
    std::memset(Register, 0, sizeof Register);
    PC = 0;
    opcode = dest = src1 = src2 = 0;
    FlagN = FlagZ = FlagC = FlagV = 0;
    end_of_simulation = 0;
    failed = 0;
    steps = 0;
}

/* fetch(): "reads four bytes from the PC address of instruction memory and stores
 * in opcode, dest, src1, and src2 respectively"; PC then points to the next instruction. */
void fetch() {
    instr_pc = PC;
    if (PC < 0 || PC > INSTR_SIZE - 4) {          /* a branch (or a hand-written .byte) left the program */
        processor_error("PC %d is outside the instruction memory (0..%d)", PC, INSTR_SIZE - 1);
        opcode = dest = src1 = src2 = 0;          /* execute() then just ends the simulation */
        return;
    }
    opcode = Instruction[PC];
    dest   = Instruction[PC + 1];
    src1   = Instruction[PC + 2];
    src2   = Instruction[PC + 3];
    PC += 4;
}

/* decode(): "void and empty function at this time" - fetch() already split the fields. */
void decode() {}

/* A word in Data[] is 4 bytes, least significant first (little-endian). */
static bool load_word(int addr, int &value) {
    if (addr < 0 || addr > DATA_SIZE - 4) {
        processor_error("data address %d is outside Data[%d] (a word needs an address 0..%d)", addr, DATA_SIZE, DATA_SIZE - 4);
        return false;
    }
    uint32_t v = 0;
    for (int k = 3; k >= 0; k--) v = (v << 8) | Data[addr + k];
    value = (int)v;
    return true;
}

static void store_word(int addr, int value) {
    if (addr < 0 || addr > DATA_SIZE - 4) {
        processor_error("data address %d is outside Data[%d] (a word needs an address 0..%d)", addr, DATA_SIZE, DATA_SIZE - 4);
        return;
    }
    uint32_t v = (uint32_t)value;
    for (int k = 0; k < 4; k++) Data[addr + k] = (unsigned char)(v >> (8 * k));
    if (addr + 4 > data_size) data_size = addr + 4;          /* finalize() writes up to here */
}

/* handout: "Every addition/subtraction operation updates the following four global flags" */
static void set_add_flags(uint32_t a, uint32_t b, uint32_t r) {
    FlagZ = (r == 0);                                            /* result is 0 */
    FlagN = (int)(r >> 31);                                      /* MSB of the result */
    FlagC = (r < a) || (r < b);                                  /* unsigned result smaller than an input */
    FlagV = ((a >> 31) == (b >> 31)) && ((r >> 31) != (a >> 31));   /* same signs in, other sign out */
}

static void set_sub_flags(uint32_t a, uint32_t b, uint32_t r) {
    FlagZ = (r == 0);
    FlagN = (int)(r >> 31);
    FlagC = (a > b);                                             /* operand 1 bigger than operand 2 (unsigned) */
    FlagV = ((a >> 31) != (b >> 31)) && ((r >> 31) == (b >> 31));   /* signs differ, result has operand 2's sign */
}

/* handout: suffix table (ARM condition codes) */
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

/* execute(): "based on opcode, it performs the operations. Once see 0 opcode,
 * set end_of_simulation = 1". */
void execute() {
    int *x = Register;
    uint32_t a = (uint32_t)x[src1], b;      /* unsigned: 32-bit wrap-around, no overflow UB */
    int value;
    steps++;
    switch (opcode) {
    case OP_HALT:
        end_of_simulation = 1;
        break;
    case OP_ADD: case OP_ADDI:                                    /* 01 D A B, 09 D A c */
        b = opcode == OP_ADD ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a + b);
        set_add_flags(a, b, a + b);
        break;
    case OP_SUB: case OP_SUBI:                                    /* 02 D A B, 0A D A c */
        b = opcode == OP_SUB ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a - b);
        set_sub_flags(a, b, a - b);
        break;
    case OP_MUL: case OP_MULI:                                    /* 03 D A B, 0B D A c */
        b = opcode == OP_MUL ? (uint32_t)x[src2] : (uint32_t)src2;
        x[dest] = (int)(a * b);
        break;
    case OP_DIV: case OP_DIVI: {                                  /* 04 D A B, 0C D A c; rounds toward 0 */
        int divisor = opcode == OP_DIV ? x[src2] : src2;
        if (divisor == 0) {
            if (opcode == OP_DIV) processor_error("division by zero (x%d is 0)", src2);
            else                  processor_error("division by zero (constant 0)");
            break;
        }
        x[dest] = (x[src1] == INT_MIN && divisor == -1) ? INT_MIN : x[src1] / divisor;
        break;
    }
    case OP_MOV:    x[dest] = x[src2]; break;                                 /* 07 D 0 A: xD = xA   */
    case OP_MOVC:   x[dest] = src2; break;                                    /* 0F D 0 c: xD = c    */
    case OP_LOAD:   if (load_word(x[src2], value)) x[dest] = value; break;    /* 05 D 0 A: xD = [xA] */
    case OP_LOADC:  if (load_word(src2, value)) x[dest] = value; break;       /* 0D D 0 c: xD = [c]  */
    case OP_STORE:  store_word(x[dest], x[src2]); break;                      /* 06 A 0 S: [xA] = xS */
    case OP_STOREC: store_word(dest, x[src2]); break;                         /* 0E c 0 S: [c] = xS  */
    default:
        if (opcode >= OP_BRANCH && opcode < OP_BRANCH + NUM_CONDITIONS) {   /* 1k 0 0 offset */
            if (condition_holds(opcode - OP_BRANCH))
                PC = instr_pc + 4 * (signed char)src2;   /* "next instruction address = current + offset" (in instructions) */
        } else {
            processor_error("unknown opcode 0x%02X", opcode);
        }
    }
    if (!end_of_simulation && steps >= MAX_STEPS)
        processor_error("%ld instructions without reaching the end: endless loop?", MAX_STEPS);
}
