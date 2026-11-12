/* CS527 Lab 3: sizes, limits and opcodes shared by every module.
 * Change a size here and rebuild (make clean && make); nothing else hard-codes it.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* ---- registers ---- */
#define NUM_XREGS   256     /* integer registers x0..x255, 32 bits each */
#define NUM_VREGS   32      /* Lab 3: vector registers v0..v31 */
#define VLEN        8       /* Lab 3: 32-bit lanes per vector register (8 x 32 = 256 bits) */

/* ---- memory (handout: char Instruction[256], Data[4096]) ---- */
#define INSTR_SIZE  256     /* instruction memory in bytes = 64 instructions of 4 bytes */
#define DATA_SIZE   4096    /* data memory in bytes */
#define MAX_INSTRUCTIONS (INSTR_SIZE / 4)
#define MAX_STEPS   10000000L   /* a program that runs longer is stopped (endless-loop guard) */

/* ---- opcodes (first byte of every 4-byte instruction) ----
 * Format: <opcode> <dest> <operand 1> <operand 2>. "c" = constant 0..255.
 */
enum Opcode {
    OP_HALT   = 0x00,   /* end of program                        00 0 0 0 */
    OP_ADD    = 0x01,   /* xD = xA + xB                          01 D A B */
    OP_SUB    = 0x02,   /* xD = xA - xB                          02 D A B */
    OP_MUL    = 0x03,   /* xD = xA * xB                          03 D A B */
    OP_DIV    = 0x04,   /* xD = xA / xB  (signed)                04 D A B */
    OP_LOAD   = 0x05,   /* xD = [xA]                             05 D 0 A */
    OP_STORE  = 0x06,   /* [xA] = xS                             06 A 0 S */
    OP_MOV    = 0x07,   /* xD = xA                               07 D 0 A */
    OP_ADDI   = 0x09,   /* xD = xA + c                           09 D A c */
    OP_SUBI   = 0x0A,   /* xD = xA - c                           0A D A c */
    OP_MULI   = 0x0B,   /* xD = xA * c                           0B D A c */
    OP_DIVI   = 0x0C,   /* xD = xA / c                           0C D A c */
    OP_LOADC  = 0x0D,   /* xD = [c]   (handout says 0x0C, which clashes with DIVI) */
    OP_STOREC = 0x0E,   /* [c] = xS                              0E c 0 S */
    OP_MOVC   = 0x0F,   /* xD = c                                0F D 0 c */
    OP_BRANCH = 0x10,   /* 0x10 + condition: B<cond> .label      1k 0 0 offset */
    /* ---- Lab 3: vector instructions (8 lanes of 32 bits; they do not change the flags) ---- */
    OP_VADD   = 0x21,   /* vD = vA + vB                          21 D A B */
    OP_VSUB   = 0x22,   /* vD = vA - vB                          22 D A B */
    OP_VMUL   = 0x23,   /* vD = vA * vB                          23 D A B */
    OP_VLOAD  = 0x25,   /* vD = [xA]  (8 words from address xA)  25 D 0 A */
    OP_VSTORE = 0x26,   /* [xA] = vS                             26 A 0 S */
    OP_VADDI  = 0x29,   /* vD = vA + c                           29 D A c */
    OP_VSUBI  = 0x2A,   /* vD = vA - c                           2A D A c */
    OP_VMULI  = 0x2B,   /* vD = vA * c                           2B D A c */
    OP_VLOADC = 0x2C,   /* vD = [c]                              2C D 0 c */
    OP_VSTOREC= 0x2E,   /* [c] = vS                              2E c 0 S */
    OP_VADDX  = 0x31,   /* vD = vA + xB  (the handout allows an integer register but gives no opcode) */
    OP_VSUBX  = 0x32,   /* vD = vA - xB                          32 D A B */
    OP_VMULX  = 0x33    /* vD = vA * xB                          33 D A B */
};

/* Branch conditions (ARM order): BEQ = 0x10 and BAL = 0x1E, as in the handout's example. */
enum Condition {
    COND_EQ = 0, COND_NE, COND_CS, COND_CC, COND_MI, COND_PL, COND_VS, COND_VC,
    COND_HI, COND_LS, COND_GE, COND_LT, COND_GT, COND_LE, COND_AL, NUM_CONDITIONS
};

#endif
