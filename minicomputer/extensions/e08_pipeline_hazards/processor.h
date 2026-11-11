/* Processor: fetch / decode / execute.
 * Lab 4 made every piece of state an array indexed by processor id (NP processors);
 * proc_id selects the processor that process_instructions() is currently running.
 */
#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <cstdio>
#include <string>                                /* EXT e08 */
#include <vector>                                /* EXT e08 */
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
extern int data_high[NP];                      /* highest data byte written + 1 (sizes the output file) */

/* EXT e08: 5-stage pipeline statistics of the task on processor p (cleared by reset()) */
struct PipeStats {
    long instructions = 0;       /* executed instructions (halt included) */
    long taken_branches = 0;     /* each one costs PIPE_BRANCH_PENALTY cycles */
    long raw_d1 = 0, raw_d2 = 0; /* RAW hazards: reader 1 or 2 pipeline slots after the writer */
    long load_use = 0;           /* distance-1 hazards on a register written by a load */
    long stalls_nofwd = 0;       /* stall cycles without forwarding (wait for the writer's WB) */
    long stalls_fwd = 0;         /* stall cycles with forwarding (only load-use stalls) */
};
extern PipeStats pipe_stats[NP];
extern std::vector<std::string> pipe_log[NP];  /* EXT e08: the first PIPE_LOG_ROWS instructions, one line each */

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
