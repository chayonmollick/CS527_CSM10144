/* Processor (handout: Processor specification): the registers, PC and the
 * reset / fetch / decode / execute cycle. Every value is 32 bits (handout: Data size,
 * "32 bit operation size is preferable").
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

/* reset(): "initializes each element of register array to 0 as well initialize PC to 0" */
void reset() {
    std::memset(Register, 0, sizeof Register);
    PC = 0;
    opcode = dest = src1 = src2 = 0;
    end_of_simulation = 0;
    failed = 0;
    steps = 0;
}

/* fetch(): "reads four bytes from the PC address of instruction memory and stores
 * in opcode, dest, src1, and src2 respectively"; PC then points to the next instruction. */
void fetch() {
    instr_pc = PC;
    if (PC < 0 || PC > INSTR_SIZE - 4) {         /* only a hand-written .byte without an end can get here */
        processor_error("PC %d is outside the instruction memory (0..%d)", PC, INSTR_SIZE - 1);
        opcode = dest = src1 = src2 = 0;         /* execute() then just ends the simulation */
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

/* execute(): "based on opcode, it performs the operations. Once see 0 opcode,
 * set end_of_simulation = 1". */
void execute() {
    int *x = Register;
    uint32_t a = (uint32_t)x[src1], b = (uint32_t)x[src2];   /* unsigned: 32-bit wrap-around, no overflow UB */
    int value;
    steps++;
    switch (opcode) {
    case OP_HALT:  end_of_simulation = 1; break;                       /* 0 0 0 0 */
    case OP_ADD:   x[dest] = (int)(a + b); break;                      /* 1 D A B */
    case OP_SUB:   x[dest] = (int)(a - b); break;                      /* 2 D A B */
    case OP_MUL:   x[dest] = (int)(a * b); break;                      /* 3 D A B */
    case OP_DIV:                                                        /* 4 D A B, rounds toward 0 */
        if (x[src2] == 0) { processor_error("division by zero (x%d is 0)", src2); break; }
        x[dest] = (x[src1] == INT_MIN && x[src2] == -1) ? INT_MIN : x[src1] / x[src2];
        break;
    case OP_READ:  if (load_word(src1, value)) x[dest] = value; break; /* 5 D a 0: Read xD, a  */
    case OP_WRITE: store_word(src1, x[dest]); break;                   /* 6 S a 0: Write xS, a */
    case OP_MOVE:  x[dest] = src1; break;                              /* 7 D c 0: xD = c      */
    default:       processor_error("unknown opcode %d", opcode); break;
    }
}
