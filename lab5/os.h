/* Operating system (handout: Operating System (OS)): process table, scheduler, shell,
 * loader, and the Lab 5 memory management unit (page tables and the free-frame list).
 */
#ifndef OS_H
#define OS_H

#include <string>

/* Handout, Memory management related functions: logical -> physical address for the task on
 * processor proc_id. isFetch = 1 for instruction fetches (instruction pages), 0 for data
 * accesses (data pages). Returns -1 (after stopping the task) on an invalid address. */
int getPhysicalAddress(int proc_id, int isFetch, int address);

int  getFreePage();                     /* first free frame (marked allocated), -1 if none */
void os_init(bool batch);               /* batch = no shell: stop when the tasks finish */
int  create_task(const std::string &program, const std::string &data);   /* returns pid, -1 on error */
void scheduler();                       /* runs until the shell is closed and every task finished */
void shell();                           /* non-blocking: handles whatever is waiting on stdin */
int  map_pid_proc_id(int pid);          /* processor running pid, -1 if none */
int  failed_task_count();

#endif
