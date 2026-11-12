/* CS527 Lab 2 mini-computer: sizes and opcodes shared by the compiler, processor and memory.
 * Change a size here and rebuild (make clean && make); nothing else hard-codes it.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* handout: Processor specification - "Int Register[256]", all registers 32 bits */
#define NUM_REGS         256
/* handout: Memory - "Char Instruction[256]; Char Data[4096];" (Lab 1 had Data[256]) */
#define INSTR_SIZE       256
#define DATA_SIZE        4096
#define MAX_INSTRUCTIONS (INSTR_SIZE / 4)    /* 64, including the end-of-program instruction */
#define MAX_STEPS        10000000L           /* a program still running after this many: endless loop */

/* handout: Byte code specification. Every instruction is four bytes:
 *     <opcode> <dest register> <operand 1> <operand 2>          branch: <opcode> 0 0 <offset>
 * "In case of memory read, write and data movement operations - operand 1 is 0."
 * D, A, B, S = register numbers, c = constant 0..255.
 */
enum Opcode {
    OP_HALT   = 0x00,   /* end of program, added by the compiler        00 0 0 0 */
    OP_ADD    = 0x01,   /* xD = xA + xB                                 01 D A B */
    OP_SUB    = 0x02,   /* xD = xA - xB                                 02 D A B */
    OP_MUL    = 0x03,   /* xD = xA * xB                                 03 D A B */
    OP_DIV    = 0x04,   /* xD = xA / xB                                 04 D A B */
    OP_LOAD   = 0x05,   /* xD = [xA]     memory read, address in xA     05 D 0 A */
    OP_STORE  = 0x06,   /* [xA] = xS     memory write, address in xA    06 A 0 S */
    OP_MOV    = 0x07,   /* xD = xA       data movement                  07 D 0 A */
    OP_ADDI   = 0x09,   /* xD = xA + c                                  09 D A c */
    OP_SUBI   = 0x0A,   /* xD = xA - c                                  0A D A c */
    OP_MULI   = 0x0B,   /* xD = xA * c                                  0B D A c */
    OP_DIVI   = 0x0C,   /* xD = xA / c                                  0C D A c */
    OP_LOADC  = 0x0D,   /* xD = [c], Read xD c  (the handout's table says 0x0C,
                           which is already divide by a constant)       0D D 0 c */
    OP_STOREC = 0x0E,   /* [c] = xS, Write xS c                         0E c 0 S */
    OP_MOVC   = 0x0F,   /* xD = c        data movement                  0F D 0 c */
    OP_BRANCH = 0x10    /* 0x10 + condition: B<cond> .label             1k 0 0 offset */
};

/* Branch conditions (handout: suffix table, ARM order): BEQ = 0x10 ... BAL = 0x1E,
 * as in the handout's example byte code. */
enum Condition {
    COND_EQ = 0, COND_NE, COND_CS, COND_CC, COND_MI, COND_PL, COND_VS, COND_VC,
    COND_HI, COND_LS, COND_GE, COND_LT, COND_GT, COND_LE, COND_AL, NUM_CONDITIONS
};

#endif
