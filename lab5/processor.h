/* Processor (handout: Processor specification): fetch / decode / execute.
 * Every piece of state is an array indexed by processor id (NP processors, from Lab 4);
 * proc_id selects the processor that process_instructions() is currently running.
 * Lab 5: the processor has no memory arrays of its own. Every address it uses (PC and data
 * addresses) is logical and goes through getPhysicalAddress() in os.cpp before memory[].
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstdio>
#include "config.h"

extern int Register[NP][NUM_XREGS];            /* x0..x255 */
extern int VRegister[NP][NUM_VREGS][VLEN];     /* v0..v31, 8 lanes each */
extern int PC[NP];                             /* logical byte address of the next instruction */
extern int FlagN[NP], FlagZ[NP], FlagC[NP], FlagV[NP];   /* set by add/sub */
extern int end_of_simulation[NP];              /* 1 once the task on processor p halted or failed */
extern int failed[NP];                         /* 1 if it stopped because of an error */
extern int current_pid[NP];                    /* task running on processor p */
extern long steps[NP];                         /* instructions executed by the current task */
extern int data_high[NP];                      /* highest data byte written + 1 (sizes the output file) */

extern int proc_id, opcode, dest, src1, src2;  /* the instruction being executed */
extern FILE *fd_log;                           /* print output */
extern int trace;                              /* 1 = print every executed instruction to stderr */

void reset(int p);
void fetch();
void decode();
void execute();
int  process_instructions(int p, int count);   /* run up to count instructions; returns how many ran */
void processor_error(const char *fmt, ...);    /* report an error and stop the current task */

#endif
