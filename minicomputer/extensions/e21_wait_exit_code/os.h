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
int  create_task(const std::string &program, const std::string &data);   /* returns pid, -1 on error */
void scheduler();                       /* runs until the shell is closed and every task finished */
void shell();                           /* non-blocking: handles whatever is waiting on stdin */
int  map_pid_proc_id(int pid);          /* processor running pid, -1 if none */
int  failed_task_count();
int  os_fork(int p, int dest);          /* EXT e21: fork() for the task on processor p: child pid, -1 on failure */
void os_wait(int p, int dest, int status_reg);   /* EXT e21: wait() for the task on processor p;
                                           status_reg = register for the child's exit code, -1 = none */

#endif
