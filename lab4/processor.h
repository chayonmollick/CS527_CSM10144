/* Processor (handout "Processor specification"): reset / fetch / decode / execute.
 * Lab 4: NP processors. Every piece of processor state is an array indexed by processor id.
 * The OS calls process_instructions(p, n); it sets proc_id = p, and fetch/decode/execute
 * then use processor p's registers and its memory Instruction[p] / Data[p].
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstdio>
#include "config.h"

extern int Register[NP][NUM_XREGS];            /* x0..x255 of each processor */
extern int VRegister[NP][NUM_VREGS][VLEN];     /* v0..v31, 8 lanes each (Lab 3) */
extern int PC[NP];                             /* byte address of the next instruction in Instruction[p] */
extern int FlagN[NP], FlagZ[NP], FlagC[NP], FlagV[NP];   /* set by add and subtract */
extern int end_of_simulation[NP];              /* 1 once the task on processor p ended (or failed) */
extern int failed[NP];                         /* 1 if it stopped because of an error */
extern int current_pid[NP];                    /* pid of the task on processor p (print, messages) */
extern long steps[NP];                         /* instructions the task has executed */

extern int proc_id, opcode, dest, src1, src2;  /* processor being run, instruction being executed */
extern FILE *fd_log;                           /* log file for print, opened in append mode */

void reset(int p);                             /* registers, flags, PC of processor p = 0; open the log */
void fetch();
void decode();
void execute();
int  process_instructions(int p, int count);   /* run up to count instructions on p, then sleep */

#endif
