/* Processor: fetch / decode / execute.
 * Lab 4 made every piece of state an array indexed by processor id (NP processors);
 * proc_id selects the processor that process_instructions() is currently running.
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstdio>
#include "config.h"

extern int Register[NP][NUM_XREGS];            /* x0..x255 */
extern int VRegister[NP][NUM_VREGS][VLEN];     /* v0..v31, 8 lanes each (Lab 3) */
extern int PC[NP];                             /* logical byte address of the next instruction */
extern int FlagN[NP], FlagZ[NP], FlagC[NP], FlagV[NP];   /* set by add/sub (Lab 2) */
extern int end_of_simulation[NP];              /* 1 once the task on processor p halted or failed */
extern int failed[NP];                         /* 1 if it stopped because of an error */
extern int PTBR[NP];                           /* Lab 6: physical address of p's page table, -1 = none */
extern int current_pid[NP];                    /* task running on processor p */
extern long steps[NP];                         /* instructions executed by the current task */
extern long exec_count[NP][MAX_INSTRUCTIONS];  /* Lab 6 task 1: times each instruction executed */
extern long exec_cycles[NP][MAX_INSTRUCTIONS]; /* EXT e09: estimated cycles spent in each instruction */
extern int data_high[NP];                      /* highest data byte written + 1 (sizes the output file) */

extern int proc_id, opcode, dest, src1, src2;  /* the instruction being executed */
extern FILE *fd_log;                           /* print output (Lab 4) */
extern int trace;                              /* 1 = print every executed instruction to stderr */

void reset(int p);
void fetch();
void decode();
void execute();
int  process_instructions(int p, int count);   /* run up to count instructions; returns how many ran */
void processor_error(const char *fmt, ...);    /* report an error and stop the current task */

#endif
