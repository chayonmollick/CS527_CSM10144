/* CS527 Lab 1 mini-computer: sizes and opcodes shared by the compiler, processor and memory.
 * Change a size here and rebuild (make clean && make); nothing else hard-codes it.
 */
#ifndef CONFIG_H
#define CONFIG_H

/* handout: Processor specification - "Int Register[256]", variables x0..x255 */
#define NUM_REGS         256
/* handout: Memory - "Char Instruction[256]; Char Data[256];" */
#define INSTR_SIZE       256
#define DATA_SIZE        256
#define MAX_INSTRUCTIONS (INSTR_SIZE / 4)    /* 64, including the end-of-program instruction */

/* handout: Byte code specification. Every instruction is four bytes, each 0..255:
 *     <operation> <dest register> <operand 1> <operand 2>
 * "In case of memory read, write and data movement operations - operand 2 is 0."
 * D, A, B, S = register numbers, a = data address, c = constant.
 */
enum Opcode {
    OP_HALT  = 0,   /* end of program, added by the compiler        0 0 0 0 */
    OP_ADD   = 1,   /* xD = xA + xB                                 1 D A B */
    OP_SUB   = 2,   /* xD = xA - xB                                 2 D A B */
    OP_MUL   = 3,   /* xD = xA * xB                                 3 D A B */
    OP_DIV   = 4,   /* xD = xA / xB                                 4 D A B */
    OP_READ  = 5,   /* Read xD, a   memory read:  xD = word at a    5 D a 0 */
    OP_WRITE = 6,   /* Write xS, a  memory write: word at a = xS    6 S a 0 */
    OP_MOVE  = 7    /* xD = c       data movement                   7 D c 0 */
};

#endif
