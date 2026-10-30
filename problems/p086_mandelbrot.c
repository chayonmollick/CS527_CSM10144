/* P086: Mandelbrot set - escape-time image and area by random sampling
 * Problem: (Textbook Ex 22.1.) (a) For a W x H image of the region
 *   [-2, 0.5] x [-1.25, 1.25] of the complex plane, compute for every pixel c
 *   the number of iterations of z = z^2 + c (z0 = 0) until |z| > 2, at most
 *   max_iter (escape time), stored row-major img[(long)y*W + x]. The work per
 *   pixel varies wildly (points inside the set take all max_iter iterations),
 *   so compare schedule(static), schedule(dynamic, 1) and schedule(guided)
 *   over rows. (b) Estimate the area of the Mandelbrot set by sampling random
 *   points in the same box: area ~ box area * (points that never escape) /
 *   (number of samples); the true area is about 1.506. Verify the image
 *   against the serial code exactly and the area within a tolerance.
 * Solution: (a) every pixel is independent (it only writes img[y][x]), so a
 *   parallel for over rows is race-free; the question is load balance. Rows
 *   through the middle of the set cost up to max_iter per pixel, rows near the
 *   top/bottom almost nothing: static gives each thread one contiguous block
 *   of rows, so the threads holding the middle blocks finish last. dynamic, 1
 *   hands out one row at a time to whichever thread is free; guided starts with
 *   big chunks and shrinks them. Each pixel uses the same code as the serial
 *   version, so the image is identical.
 *   (b) parallel for with reduction(+:inside). Random numbers: no rand() (not
 *   thread-safe, shared state). Sample k uses a counter-based generator: its
 *   two coordinates are a hash of (2k) and (2k+1). The sample set does not
 *   depend on the number of threads or the schedule, so the parallel count
 *   equals the serial count exactly.
 * Source: textbook Ex 22.1 (Mandelbrot).
 * Build: gcc -O2 -Wall -Wextra -fopenmp p086_mandelbrot.c -o p086 -lm
 * Run:   ./p086 [W] [H] [max_iter] [samples]   (defaults 500, 500, 500, 3e5)
 */
#include "common.h"

#define XMIN -2.0
#define XMAX  0.5
#define YMIN -1.25
#define YMAX  1.25
#define BOX_AREA ((XMAX - XMIN) * (YMAX - YMIN))       /* 6.25 */

/* Escape time of c = cr + i*ci: iterations until |z| > 2, at most max_iter. */
static inline int escape_time(double cr, double ci, int max_iter) {
    double zr = 0.0, zi = 0.0;
    int it = 0;
    while (it < max_iter && zr * zr + zi * zi <= 4.0) {
        double t = zr * zr - zi * zi + cr;
        zi = 2.0 * zr * zi + ci;
        zr = t;
        it++;
    }
    return it;
}

/* Pixel (x, y) -> centre of the pixel in the complex plane. */
static inline int pixel_value(long x, long y, long W, long H, int max_iter) {
    double cr = XMIN + ((double)x + 0.5) * (XMAX - XMIN) / (double)W;
    double ci = YMIN + ((double)y + 0.5) * (YMAX - YMIN) / (double)H;
    return escape_time(cr, ci, max_iter);
}

static void image_serial(int *img, long W, long H, int max_iter) {
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) img[y * W + x] = pixel_value(x, y, W, H, max_iter);
}
static void image_static(int *img, long W, long H, int max_iter) {
    #pragma omp parallel for schedule(static)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) img[y * W + x] = pixel_value(x, y, W, H, max_iter);
}
static void image_dynamic(int *img, long W, long H, int max_iter) {
    #pragma omp parallel for schedule(dynamic, 1)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) img[y * W + x] = pixel_value(x, y, W, H, max_iter);
}
static void image_guided(int *img, long W, long H, int max_iter) {
    #pragma omp parallel for schedule(guided)
    for (long y = 0; y < H; y++)
        for (long x = 0; x < W; x++) img[y * W + x] = pixel_value(x, y, W, H, max_iter);
}

/* Counter-based random number in [0, 1): splitmix64 hash of k (no shared state). */
static inline double u01(unsigned long long k) {
    unsigned long long z = k * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return (double)(z >> 11) * (1.0 / 9007199254740992.0);    /* 53 random bits */
}

/* Sample k is inside if it does not escape within max_iter. */
static inline int sample_inside(long k, int max_iter) {
    double cr = XMIN + (XMAX - XMIN) * u01(2 * (unsigned long long)k);
    double ci = YMIN + (YMAX - YMIN) * u01(2 * (unsigned long long)k + 1);
    return escape_time(cr, ci, max_iter) == max_iter;
}

static long area_serial(long samples, int max_iter) {
    long inside = 0;
    for (long k = 0; k < samples; k++) inside += sample_inside(k, max_iter);
    return inside;
}
static long area_parallel(long samples, int max_iter) {
    long inside = 0;
    #pragma omp parallel for schedule(dynamic, 1000) reduction(+:inside)
    for (long k = 0; k < samples; k++) inside += sample_inside(k, max_iter);
    return inside;
}

int main(int argc, char *argv[]) {
    long W = arg_long(argc, argv, 1, 500);
    long H = arg_long(argc, argv, 2, 500);
    int max_iter = (int)arg_long(argc, argv, 3, 500);
    long samples = arg_long(argc, argv, 4, 300000);
    if (W < 1) W = 1;
    if (H < 1) H = 1;
    if (max_iter < 1) max_iter = 1;
    if (samples < 1) samples = 1;

    /* Worked example: 64 x 24 ASCII picture ('#' = inside the set). */
    enum { AW = 64, AH = 24 };
    int pic[AW * AH];
    image_dynamic(pic, AW, AH, 200);
    printf("P086: Mandelbrot set\n");
    for (int y = 0; y < AH; y++) {
        for (int x = 0; x < AW; x++) {
            int v = pic[y * AW + x];
            putchar(v == 200 ? '#' : v > 20 ? '+' : v > 6 ? '.' : ' ');
        }
        putchar('\n');
    }

    int *ims = xmalloc((size_t)W * H * sizeof(int)), *imp = xmalloc((size_t)W * H * sizeof(int));
    omp_warmup();
    double ts, tp;
    BEST_TIME(ts, image_serial(ims, W, H, max_iter));
    long row_min = -1, row_max = 0;                    /* work (iterations) per row */
    for (long y = 0; y < H; y++) {
        long r = 0;
        for (long x = 0; x < W; x++) r += ims[y * W + x];
        if (row_min < 0 || r < row_min) row_min = r;
        if (r > row_max) row_max = r;
    }
    printf("\n(a) %ld x %ld image, max_iter = %d; iterations per row from %ld to %ld\n",
           W, H, max_iter, row_min, row_max);
    const char *label[3] = {"schedule(static)", "schedule(dynamic, 1)", "schedule(guided)"};
    for (int v = 0; v < 3; v++) {
        printf("%s (check: image equal to serial exactly)\n", label[v]);
        table_header();
        for (int k = 0; k < NTHREADS; k++) {
            omp_set_num_threads(THREADS[k]);
            memset(imp, 0, (size_t)W * H * sizeof(int));
            if (v == 0) BEST_TIME(tp, image_static(imp, W, H, max_iter));
            if (v == 1) BEST_TIME(tp, image_dynamic(imp, W, H, max_iter));
            if (v == 2) BEST_TIME(tp, image_guided(imp, W, H, max_iter));
            table_row(W * H, THREADS[k], ts, tp, same_ints(ims, imp, W * H));
        }
    }

    long in_s = 0, in_p = 0;
    BEST_TIME(ts, in_s = area_serial(samples, max_iter));
    double area = BOX_AREA * (double)in_s / (double)samples;
    double p0 = 1.5066 / BOX_AREA;                        /* true fraction inside */
    double sigma = BOX_AREA * sqrt(p0 * (1.0 - p0) / (double)samples);   /* std. error */
    printf("\n(b) area by sampling: %ld samples, %ld inside -> area = %.4f, std. error %.4f "
           "(true area ~ 1.5066)  %s\n", samples, in_s, area, sigma,
           ok_str(fabs(area - 1.5066) < 0.03 + 4.0 * sigma));
    printf("(check: same count as serial; finite max_iter makes the estimate a little too big)\n");
    table_header();
    for (int k = 0; k < NTHREADS; k++) {
        omp_set_num_threads(THREADS[k]);
        BEST_TIME(tp, in_p = area_parallel(samples, max_iter));
        table_row(samples, THREADS[k], ts, tp, in_p == in_s);
    }
    free(ims); free(imp);
    return 0;
}

/* Notes:
 * - Load imbalance: the cost of a row = total iterations of its pixels, which
 *   ranges from ~W (top rows) to ~W*max_iter (middle rows). static blocks put
 *   all the heavy rows on the middle threads.
 * - dynamic,1: perfect balance at the cost of one shared-counter update per
 *   row (cheap compared with W pixels). guided: fewer scheduling events,
 *   good when the heavy part is not at the start.
 * - Random sampling in parallel: rand() has one hidden shared state (race,
 *   not reproducible). Use a per-thread generator (seed from the thread
 *   number, e.g. rand_r or the LCG in common.h) or, as here, a counter-based
 *   hash of the sample index (result independent of the thread count).
 * - Sampling error ~ 1/sqrt(samples); a larger max_iter reduces the bias
 *   (points that escape late are counted as inside).
 */
