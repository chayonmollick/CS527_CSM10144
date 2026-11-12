/* Operating system (handout "Operating System (OS)"): process table, ready and waiting
 * queues, PID -> processor map, round-robin scheduler, non-blocking shell and loader.
 */
#ifndef OS_H
#define OS_H

#include <string>

void os_init(bool batch);               /* batch = no shell: stop when the tasks finish */
int  create_task(const std::string &program, const std::string &data);   /* returns pid, -1 on error */
void scheduler();                       /* runs until the shell is closed and every task has finished */
void shell();                           /* non-blocking: handles whatever is waiting on stdin */
int  map_pid_proc_id(int pid);          /* processor running pid, -1 if none */
int  failed_task_count();

#endif
