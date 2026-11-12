/* 15_tasks_fibonacci.c - tasks for recursion: fib(n) = fib(n-1) + fib(n-2)
 *
 * What: "#pragma omp task" packages a piece of work that any thread of the team may run
 *       later. "#pragma omp taskwait" waits for the tasks that THIS task created.
 *       The pattern: parallel { single { first call } } - one thread starts the
 *       recursion and creates tasks; all the threads of the team run them.
 *       Data: inside a task, a local variable of the parent is firstprivate (a copy) by
 *       default; shared(x) makes the child write the parent's x itself.
 * When: recursion / divide and conquer (merge sort, quicksort, tree walks) and work
 *       whose amount is not known in advance (linked lists). Always use a CUTOFF: a task
 *       costs about a microsecond, so below the cutoff call the serial function.
 * Expected output (4 threads; the times VARY, the values and the task count do not):
 *   fib(32) serial = 2178309   0.0021 s
 *   fib(32) tasks  = 2178309   0.0010 s   (1218 tasks, cutoff 20)
 *   tasks give the serial answer: OK
 */
#include <omp.h>
#include <stdio.h>

#define CUTOFF 20                           /* below this n: plain recursion, no tasks */

static long tasks = 0;

static long fib_serial(int n) { return n < 2 ? n : fib_serial(n - 1) + fib_serial(n - 2); }

static long fib(int n) {
    if (n < CUTOFF) return fib_serial(n);   /* too small to be worth a task */
    long x = 0, y = 0;
    #pragma omp atomic
    tasks += 2;
    #pragma omp task shared(x)              /* the child writes the parent's x */
    x = fib(n - 1);
    #pragma omp task shared(y)
    y = fib(n - 2);
    #pragma omp taskwait                    /* both children done: x and y are ready */
    return x + y;
}

int main(void) {
    int n = 32;
    double t0 = omp_get_wtime();
    long s = fib_serial(n);
    double t1 = omp_get_wtime();
    long p = 0;
    #pragma omp parallel
    {
        #pragma omp single                  /* one thread starts the recursion ... */
        p = fib(n);                         /* ... every thread runs the tasks it creates */
    }
    double t2 = omp_get_wtime();
    printf("fib(%d) serial = %ld   %.4f s\n", n, s, t1 - t0);
    printf("fib(%d) tasks  = %ld   %.4f s   (%ld tasks, cutoff %d)\n", n, p, t2 - t1, tasks, CUTOFF);
    printf("tasks give the serial answer: %s\n", p == s ? "OK" : "WRONG");
    return p == s ? 0 : 1;
}
