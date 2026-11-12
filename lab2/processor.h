/* Processor (handout: Processor specification): "Processor has four functions:
 * reset() fetch() decode() execute()". Instructions come from Instruction[] and
 * data from Data[] (memory.h).
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include "config.h"

extern int Register[NUM_REGS];             /* handout: int Register[256] - x0..x255, 32 bits each */
extern int PC;                             /* byte address of the next instruction in Instruction[] */
extern int opcode, dest, src1, src2;       /* the four bytes fetch() read */
extern int FlagN, FlagZ, FlagC, FlagV;     /* handout: "global flags N Z C V", set by add/subtract */
extern int end_of_simulation;              /* set to 1 by opcode 0 (or by a run-time error) */
extern int failed;                         /* 1 if the simulation stopped because of an error */
extern long steps;                         /* instructions executed */

void reset();
void fetch();
void decode();
void execute();

#endif
