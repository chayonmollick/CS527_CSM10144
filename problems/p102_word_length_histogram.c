/* P102: Word-length histogram, longest word and sentence count
 * Problem: A large text of N characters is given (generated here: lowercase
 *   words separated by spaces and newlines, some followed by '.'). A word is a
 *   maximal run of letters. Compute with OpenMP: the histogram of word lengths
 *   (lengths 1..20, and one bin for 21 or more), the longest word (its length
 *   and starting position; ties -> the first one) and the number of sentences
 *   (number of '.' characters). Words may cross the boundary between two
 *   threads' chunks and must be counted exactly once. Verify against the serial
 *   code and report time and speedup for 1, 2, 4 and 8 threads.
 * Solution: position i is a word START if t[i] is a letter and t[i-1] is not (or
 *   i = 0). Only the thread that owns position i counts that word; it reads
 *   forward (past its own chunk if needed) to measure the length. A thread whose
 *   chunk begins in the middle of a word sees that t[lo-1] is a letter, so it
 *   does not count it. All reads of t are read-only, so the only shared results
 *   are the counters:
 *   (a) parallel for over positions: reduction(+:hist, sentences) for the counts
 *       (array reduction), per-thread longest word merged in a critical section.
 *   (b) explicit chunks: thread t scans [t*N/T, (t+1)*N/T) into a private Stats
 *       struct, then merges it into the shared one in a critical section.
 * Source: text-processing lab extension (chunk boundaries).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p102_word_length_histogram.c -o p102 -lm
 * Run:   ./p102 [N]          (default N = 3e7 characters)
 */
#include "common.h"

#define MAXLEN 20               /* bins 1..20, bin 21 = "21 or longer" */
#define HB (MAXLEN + 2)         /* hist[0] unused */

typedef struct {
    long hist[HB];
    long sentences;
    long best_len, best_pos;    /* longest word; ties -> smaller position */
} Stats;

static inline int is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

static void stats_init(Stats *s) {
    memset(s, 0, sizeof(Stats));
    s->best_pos = -1;
}

/* Longer word wins; equal length -> the earlier one. */
static void keep_longest(Stats *s, long len, long pos) {
    if (len > s->best_len || (len == s->best_len && pos < s->best_pos)) {
        s->best_len = len; s->best_pos = pos;
    }
}

/* Count everything that STARTS in positions lo..hi-1 (reads t[lo-1] and past hi). */
static void scan_range(const char *t, long n, long lo, long hi, Stats *s) {
    for (long i = lo; i < hi; i++) {
        if (t[i] == '.') s->sentences++;
        if (is_letter(t[i]) && (i == 0 || !is_letter(t[i - 1]))) {    /* word start */
            long len = 1;
            while (i + len < n && is_letter(t[i + len])) len++;
            s->hist[len > MAXLEN ? MAXLEN + 1 : len]++;
            keep_longest(s, len, i);
        }
    }
}

static void words_serial(const char *t, long n, Stats *s) {
    stats_init(s);
    scan_range(t, n, 0, n, s);
}

/* (a) parallel for with an array reduction; longest word per thread + critical. */
static void words_parallel_for(const char *t, long n, Stats *out) {
    long hist[HB] = {0}, sentences = 0;
    stats_init(out);
    #pragma omp parallel
    {
        Stats mine;                              /* only best_len/best_pos used here */
        stats_init(&mine);
        #pragma omp for schedule(static) reduction(+:hist, sentences)
        for (long i = 0; i < n; i++) {
            if (t[i] == '.') sentences++;
            if (is_letter(t[i]) && (i == 0 || !is_letter(t[i - 1]))) {
                long len = 1;
                while (i + len < n && is_letter(t[i + len])) len++;
                hist[len > MAXLEN ? MAXLEN + 1 : len]++;
                keep_longest(&mine, len, i);
            }
        }
        #pragma omp critical
        keep_longest(out, mine.best_len, mine.best_pos);
    }
    memcpy(out->hist, hist, sizeof(hist));
    out->sentences = sentences;
}

/* (b) explicit chunks, private Stats per thread, merged in a critical section. */
static void words_chunks(const char *t, long n, Stats *out) {
    stats_init(out);
    #pragma omp parallel
    {
        int id = omp_get_thread_num(), T = omp_get_num_threads();
        long lo = n * id / T, hi = n * (id + 1) / T;   /* this thread's chunk */
        Stats mine;
        stats_init(&mine);
        scan_range(t, n, lo, hi, &mine);
        #pragma omp critical
        {
            for (int b = 0; b < HB; b++) out->hist[b] += mine.hist[b];
            out->sentences += mine.sentences;
            keep_longest(out, mine.best_len, mine.best_pos);
        }
    }
}

/* Words of 1..12 letters (sometimes 13..30), spaces/newlines, some '.'. */
static void make_text(char *t, long n) {
    unsigned long long s = 314159;
    long i = 0;
    while (i < n) {
        unsigned long r = lcg_next(&s);
        long len = (r % 50 == 0) ? 13 + (long)(r / 50 % 18) : 1 + (long)(r / 50 % 12);
        for (long k = 0; k < len && i < n; k++) t[i++] = (char)('a' + lcg_next(&s) % 26);
        if (i < n && r / 1000 % 8 == 0) t[i++] = '.';
        if (i < n) t[i++] = (r / 8000 % 16 == 0) ? '\n' : ' ';
        if (i < n && r / 128000 % 10 == 0) t[i++] = ' ';     /* a double space */
    }
}

static int same_stats(const Stats *a, const Stats *b) {
    return same_longs(a->hist, b->hist, HB) && a->sentences == b->sentences &&
           a->best_len == b->best_len && a->best_pos == b->best_pos;
}

int main(int argc, char *argv[]) {
    long n = arg_long(argc, argv, 1, 30000000);
    if (n < 1) { fprintf(stderr, "N must be >= 1\n"); return 1; }
    char *t = xmalloc((size_t)n);
    make_text(t, n);
    omp_warmup();

    Stats ss, sp;
    double ts, tp;
    BEST_TIME(ts, words_serial(t, n, &ss));
    long words = 0;
    for (int b = 1; b < HB; b++) words += ss.hist[b];
    printf("P102: text of %ld chars: %ld words, %ld sentences, longest word %ld letters at %ld\n",
           n, words, ss.sentences, ss.best_len, ss.best_pos);
    printf("length:");
    for (int b = 1; b <= MAXLEN; b++) printf(" %ld:%ld", (long)b, ss.hist[b]);
    printf(" 21+:%ld\n", ss.hist[MAXLEN + 1]);

    printf("(a) parallel for, reduction(+:hist, sentences), longest per thread + critical\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, words_parallel_for(t, n, &sp));
        table_row(n, THREADS[k], ts, tp, same_stats(&ss, &sp));
    }
    printf("(b) explicit chunks, private Stats per thread, merged in critical\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, words_chunks(t, n, &sp));
        table_row(n, THREADS[k], ts, tp, same_stats(&ss, &sp));
    }
    free(t);
    return 0;
}

/* Notes:
 * - Boundary rule: count a word where it STARTS. Reading t[i-1] or reading past
 *   the chunk end is safe because nobody writes the text (read-only sharing).
 *   Splitting words at chunk ends instead would count one word as two pieces.
 * - reduction(+:hist) on a whole array (fixed size) works in OpenMP 4.5; for a
 *   pointer you write hist[:HB]. Each thread gets a zeroed private histogram.
 * - The longest word is a max-location (length + position), so it cannot be a
 *   built-in reduction: keep it per thread and merge once in a critical (or use
 *   declare reduction). The tie rule makes the merge order irrelevant.
 * - A sentence is counted per '.', so no boundary problem there; each character
 *   belongs to exactly one chunk.
 * - Memory-bound scan (one pass over N bytes): speedup is limited by bandwidth.
 */
