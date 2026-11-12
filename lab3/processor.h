/* Processor (handout "Processor specification"): reset / fetch / decode / execute.
 * One processor; it reads Instruction[] and reads/writes Data[] (memory.h).
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "config.h"

extern int Register[NUM_XREGS];               /* x0..x255, 32 bits each */
extern int VRegister[NUM_VREGS][VLEN];        /* Lab 3: v0..v31, 8 lanes of 32 bits each */
extern int PC;                                /* byte address of the next instruction in Instruction[] */
extern int opcode, dest, src1, src2;          /* the instruction being executed */
extern int FlagN, FlagZ, FlagC, FlagV;        /* set by add and subtract */
extern int end_of_simulation;                 /* 1 once the program reached opcode 0 (or failed) */
extern int failed;                            /* 1 if it stopped because of an error */
extern long steps;                            /* instructions executed */

void reset();     /* registers, vector registers, flags and PC = 0 */
void fetch();     /* Instruction[PC .. PC+3] -> opcode, dest, src1, src2; PC += 4 */
void decode();    /* empty: execute() uses the fields directly */
void execute();   /* perform the instruction; opcode 0 sets end_of_simulation */

#endif
