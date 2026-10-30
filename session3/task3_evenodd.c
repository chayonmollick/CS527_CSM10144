/* CS527 Computer Systems — OpenMP Lab 9
 * Task 3: Count even and odd elements of an int array.
 *  - serial
 *  - reduction(+:even, odd)        two reduction variables in one clause
 *  - #pragma omp atomic            every increment is an atomic update
 *  - per-thread counters + one atomic add per thread at the end
 * Verifies every version against serial and that even + odd == N.
 * Usage: ./task3_evenodd [N]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define REPS 3                      /* every timing is the best of REPS runs */

/* a[i] % 2 is -1 for negative odd numbers in C, so test == 0 / != 0, never == 1. */
static void count_serial(const int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0) even++;
        else               odd++;
    }
    *even_count = even; *odd_count = odd;
}

static void count_reduction(const int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;
    #pragma omp parallel for reduction(+:even, odd)
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0) even++;
        else               odd++;
    }
    *even_count = even; *odd_count = odd;
}

static void count_atomic(const int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;
    #pragma omp parallel for
    for (long i = 0; i < n; i++) {
        if (a[i] % 2 == 0) {
            #pragma omp atomic
            even++;
        } else {
            #pragma omp atomic
            odd++;
        }
    }
    *even_count = even; *odd_count = odd;
}

/* What reduction does by hand: private counters, combined once per thread. */
static void count_local(const int *a, long n, long *even_count, long *odd_count) {
    long even = 0, odd = 0;
    #pragma omp parallel
    {
        long my_even = 0, my_odd = 0;
        #pragma omp for
        for (long i = 0; i < n; i++) {
            if (a[i] % 2 == 0) my_even++;
            else               my_odd++;
        }
        #pragma omp atomic
        even += my_even;
        #pragma omp atomic
        odd += my_odd;
    }
    *even_count = even; *odd_count = odd;
}

typedef void (*count_fn)(const int *, long, long *, long *);

static double best_time(count_fn f, const int *a, long n, long *ev, long *od) {
    double best = 1e30;
    for (int r = 0; r < REPS; r++) {
        double t0 = omp_get_wtime();
        f(a, n, ev, od);
        double t = omp_get_wtime() - t0;
        if (t < best) best = t;
    }
    return best;
}

int main(int argc, char *argv[]) {
    long sizes[] = {100000L, 1000000L, 10000000L, 100000000L};
    int nsizes = 4;
    if (argc > 1) { sizes[0] = atol(argv[1]); nsizes = 1; }
    const int threads[] = {1, 2, 4, 8};
    long nmax = sizes[nsizes - 1];

    int *a = malloc((size_t)nmax * sizeof(int));
    if (!a) { fprintf(stderr, "alloc failed\n"); return 1; }
    unsigned long long x = 88172645463325252ULL;    /* signed values, roughly half even */
    for (long i = 0; i < nmax; i++) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        a[i] = (int)(x >> 32);
    }

    #pragma omp parallel
    { }                             /* warm up the thread pool */

    printf("Task 3: Even/odd count (best of %d runs, %d logical cores)\n", REPS, omp_get_num_procs());
    printf("Each cell: time(s) / speedup vs serial. ok = all versions match serial and even+odd == N.\n\n");
    printf("%10s %4s %10s | %10s %6s %5s | %10s %6s %5s | %10s %6s | %3s\n",
           "N", "thr", "serial", "reduction", "S", "E", "atomic", "S", "E", "local+atom", "S", "ok");
    for (int si = 0; si < nsizes; si++) {
        long n = sizes[si], se, so, e, o;
        double ts = best_time(count_serial, a, n, &se, &so);
        printf("%10s even = %ld, odd = %ld, even + odd = %ld (%s)\n", "", se, so, se + so,
               se + so == n ? "== N" : "!= N");
        for (int ti = 0; ti < 4; ti++) {
            int p = threads[ti];
            omp_set_num_threads(p);
            double t1 = best_time(count_reduction, a, n, &e, &o); int ok = (e == se && o == so);
            double t2 = best_time(count_atomic,    a, n, &e, &o); ok &= (e == se && o == so);
            double t3 = best_time(count_local,     a, n, &e, &o); ok &= (e == se && o == so);
            ok &= (e + o == n);
            printf("%10ld %4d %10.6f | %10.6f %6.2f %5.2f | %10.6f %6.2f %5.2f | %10.6f %6.2f | %3s\n",
                   n, p, ts, t1, ts/t1, ts/t1/p, t2, ts/t2, ts/t2/p, t3, ts/t3, ok ? "yes" : "NO");
        }
        printf("\n");
    }
    free(a);
    return 0;
}
