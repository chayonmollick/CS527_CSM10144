/* CS527 Computer Systems — OpenMP practice
 * Q15: Given a long text (char array), use OpenMP to count vowels, compute the
 * frequency of each letter a..z, count words, count occurrences of a pattern,
 * convert to upper case, and check whether a string is a palindrome.
 *  - vowels, words, pattern: reduction(+:)
 *  - letter frequency: array-section reduction(+:freq[:26])
 *  - upper case: plain parallel for (each i writes only its own char)
 *  - palindrome: reduction(&&:ok) over the first half
 * Usage: ./q15_string_ops [N] [pattern]
 */
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPS 3

static int is_vowel(char c) {
    return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u';
}

/* ---- serial ----
 * noinline: inlined into main, gcc would specialise these (constant pattern, known
 * non-overlapping buffers) and vectorise them, which the OpenMP-outlined parallel
 * loops can't do, so the 1-thread comparison would be unfair. The pattern is read
 * from argv for the same reason. */
#define NOINLINE __attribute__((noinline))
static NOINLINE long vowels_serial(const char *s, long n) {
    long v = 0;
    for (long i = 0; i < n; i++) v += is_vowel(s[i]);
    return v;
}
static NOINLINE void freq_serial(const char *s, long n, long *f) {
    for (int c = 0; c < 26; c++) f[c] = 0;
    for (long i = 0; i < n; i++) if (s[i] >= 'a' && s[i] <= 'z') f[s[i] - 'a']++;
}
/* A word starts at i when s[i] is not a space and s[i-1] is (or i == 0). */
static NOINLINE long words_serial(const char *s, long n) {
    long w = 0;
    for (long i = 0; i < n; i++) w += (s[i] != ' ' && (i == 0 || s[i-1] == ' '));
    return w;
}
static NOINLINE long pattern_serial(const char *s, long n, const char *p, long m) {
    long cnt = 0;
    for (long i = 0; i + m <= n; i++) cnt += (memcmp(s + i, p, (size_t)m) == 0);
    return cnt;
}
static NOINLINE void upper_serial(const char *s, char *u, long n) {
    for (long i = 0; i < n; i++) u[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
}
static NOINLINE int palindrome_serial(const char *s, long n) {
    for (long i = 0; i < n / 2; i++) if (s[i] != s[n-1-i]) return 0;
    return 1;
}

/* ---- parallel ---- */
static long vowels_parallel(const char *s, long n) {
    long v = 0;
    #pragma omp parallel for reduction(+:v)
    for (long i = 0; i < n; i++) v += is_vowel(s[i]);
    return v;
}
static void freq_parallel(const char *s, long n, long *f) {
    for (int c = 0; c < 26; c++) f[c] = 0;
    #pragma omp parallel for reduction(+:f[:26])
    for (long i = 0; i < n; i++) if (s[i] >= 'a' && s[i] <= 'z') f[s[i] - 'a']++;
}
/* Only reads s, so looking at s[i-1] across a chunk boundary is safe. */
static long words_parallel(const char *s, long n) {
    long w = 0;
    #pragma omp parallel for reduction(+:w)
    for (long i = 0; i < n; i++) w += (s[i] != ' ' && (i == 0 || s[i-1] == ' '));
    return w;
}
static long pattern_parallel(const char *s, long n, const char *p, long m) {
    long cnt = 0;
    #pragma omp parallel for reduction(+:cnt)
    for (long i = 0; i <= n - m; i++) cnt += (memcmp(s + i, p, (size_t)m) == 0);
    return cnt;
}
static void upper_parallel(const char *s, char *u, long n) {
    #pragma omp parallel for
    for (long i = 0; i < n; i++) u[i] = (s[i] >= 'a' && s[i] <= 'z') ? (char)(s[i] - 32) : s[i];
}
/* No early exit (break is not allowed in an omp for), so the whole half is checked. */
static int palindrome_parallel(const char *s, long n) {
    int ok = 1;
    #pragma omp parallel for reduction(&&:ok)
    for (long i = 0; i < n / 2; i++) ok = ok && (s[i] == s[n-1-i]);
    return ok;
}

/* Deterministic text: words of 1..9 lowercase letters separated by single spaces. */
static void make_text(char *s, long n) {
    unsigned long long x = 88172645463325252ULL;
    long i = 0;
    while (i < n) {
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
        int len = 1 + (int)((x >> 40) % 9);
        for (int k = 0; k < len && i < n; k++) {
            x = x * 6364136223846793005ULL + 1442695040888963407ULL;
            s[i++] = (char)('a' + (x >> 40) % 26);
        }
        if (i < n) s[i++] = ' ';
    }
}

int main(int argc, char *argv[]) {
    long n = argc > 1 ? atol(argv[1]) : 100000000L;    /* 1e8 chars = 100 MB */
    const char *pat = argc > 2 ? argv[2] : "the";      /* run-time value: not constant-folded */
    long m = (long)strlen(pat);
    const int threads[] = {1, 2, 4, 8};

    char *s  = malloc((size_t)n);
    char *us = malloc((size_t)n), *up = malloc((size_t)n);
    char *pal = malloc((size_t)n);
    if (!s || !us || !up || !pal) { fprintf(stderr, "alloc failed\n"); return 1; }
    make_text(s, n);
    for (long i = 0; i < n / 2; i++) { pal[i] = s[i]; pal[n-1-i] = s[i]; }   /* mirror -> palindrome */
    if (n % 2) pal[n / 2] = 'x';

    /* correctness, 8 threads */
    omp_set_num_threads(8);
    long fs[26], fp[26];
    freq_serial(s, n, fs); freq_parallel(s, n, fp);
    upper_serial(s, us, n); upper_parallel(s, up, n);
    int pal_yes = palindrome_parallel(pal, n), pal_no = palindrome_parallel(s, n);
    printf("Q15: string operations on N = %ld chars (8 threads for checks)\n", n);
    printf("  vowels   serial %ld parallel %ld -> %s\n", vowels_serial(s, n), vowels_parallel(s, n),
           vowels_serial(s, n) == vowels_parallel(s, n) ? "OK" : "WRONG");
    printf("  words    serial %ld parallel %ld -> %s\n", words_serial(s, n), words_parallel(s, n),
           words_serial(s, n) == words_parallel(s, n) ? "OK" : "WRONG");
    printf("  \"%s\"    serial %ld parallel %ld -> %s\n", pat, pattern_serial(s, n, pat, m),
           pattern_parallel(s, n, pat, m),
           pattern_serial(s, n, pat, m) == pattern_parallel(s, n, pat, m) ? "OK" : "WRONG");
    printf("  freq[a..z]  -> %s   (a=%ld, e=%ld, z=%ld)\n",
           memcmp(fs, fp, sizeof fs) == 0 ? "OK" : "WRONG", fs[0], fs[4], fs[25]);
    printf("  uppercase   -> %s   (first 30: %.*s)\n", memcmp(us, up, (size_t)n) == 0 ? "OK" : "WRONG",
           (int)(n < 30 ? n : 30), up);
    printf("  palindrome  -> mirrored text: %s (%s), random text: %s (%s)\n",
           pal_yes ? "yes" : "no", pal_yes == palindrome_serial(pal, n) ? "OK" : "WRONG",
           pal_no ? "yes" : "no",  pal_no == palindrome_serial(s, n) ? "OK" : "WRONG");

    /* timing: best of REPS for each operation */
    const char *names[] = {"vowels", "freq[26]", "words", "pattern", "upper", "palindrome"};
    printf("\n%-11s %10s", "op", "serial");
    for (int ti = 0; ti < 4; ti++) printf("  %6s%-2d %6s", "t=", threads[ti], "S");
    printf("\n");
    for (int op = 0; op < 6; op++) {
        double tser = 1e30, tpar[4];
        volatile long sink = 0;
        for (int r = 0; r < REPS; r++) {
            double t0 = omp_get_wtime();
            switch (op) {
            case 0: sink += vowels_serial(s, n); break;
            case 1: freq_serial(s, n, fs); break;
            case 2: sink += words_serial(s, n); break;
            case 3: sink += pattern_serial(s, n, pat, m); break;
            case 4: upper_serial(s, us, n); break;
            default: sink += palindrome_serial(pal, n);
            }
            double t = omp_get_wtime() - t0; if (t < tser) tser = t;
        }
        for (int ti = 0; ti < 4; ti++) {
            omp_set_num_threads(threads[ti]);
            tpar[ti] = 1e30;
            for (int r = 0; r < REPS; r++) {
                double t0 = omp_get_wtime();
                switch (op) {
                case 0: sink += vowels_parallel(s, n); break;
                case 1: freq_parallel(s, n, fp); break;
                case 2: sink += words_parallel(s, n); break;
                case 3: sink += pattern_parallel(s, n, pat, m); break;
                case 4: upper_parallel(s, up, n); break;
                default: sink += palindrome_parallel(pal, n);
                }
                double t = omp_get_wtime() - t0; if (t < tpar[ti]) tpar[ti] = t;
            }
        }
        printf("%-11s %10.6f", names[op], tser);
        for (int ti = 0; ti < 4; ti++) printf("  %8.6f %6.2f", tpar[ti], tser / tpar[ti]);
        printf("\n");
    }
    free(s); free(us); free(up); free(pal);
    return 0;
}

/* Viva notes:
 * - Every operation either reads shared text only (counts) or writes only its own
 *   index (upper case), so there is no race; counts are combined with reduction.
 * - Word count looks at s[i-1], which may belong to another thread's chunk. That is
 *   fine because the text is only read, never written, inside the loop.
 * - Pattern matches may overlap chunk boundaries; each start index i is still
 *   tested exactly once, so nothing is double counted or missed.
 * - Palindrome: reduction(&&:ok) gives each thread a private ok = 1 (identity of
 *   &&) and ANDs them at the end. A `break` is not allowed inside an omp for; use
 *   `#pragma omp cancel for` (OMP_CANCELLATION=true) for an early exit.
 * - These loops are memory-bound (1 byte read per cheap test), so speedup flattens
 *   once memory bandwidth is saturated; the pattern search does more work per
 *   byte and scales best.
 */
