/* Operating system (Labs 4-6): process table, scheduler, shell, loader and the
 * memory-management unit (page tables stored in physical frame 0, Lab 6 task 2).
 */
#ifndef OS_H
#define OS_H

#include <string>

/* Lab 5/6 MMU: logical -> physical address for the task on processor proc_id.
 * isFetch = 1 for instruction fetches (instruction pages), 0 for data accesses.
 * Returns -1 (after stopping the task) on an invalid address. */
int getPhysicalAddress(int proc_id, int isFetch, int address);

int  getFreePage();                     /* first free frame (marked allocated), -1 if none */
void os_init(bool batch);               /* batch = no shell: stop when the tasks finish */
int  create_task(const std::string &program, const std::string &data,
                 int priority = 0);     /* returns pid, -1 on error. EXT e16: + priority (higher first) */
void scheduler();                       /* runs until the shell is closed and every task finished */
void shell();                           /* non-blocking: handles whatever is waiting on stdin */
int  map_pid_proc_id(int pid);          /* processor running pid, -1 if none */
int  failed_task_count();

extern int sched_policy;                /* EXT e16: ROUND_ROBIN, PRIORITY or SJF (default SCHED_POLICY) */
bool set_sched_policy(const char *name);                    /* EXT e16: "rr", "priority" or "sjf" */
bool parse_priority(const std::string &s, int &priority);   /* EXT e16: "0".."9999" -> true */

#endif
