/* hwbench — hardware benchmark
 * Measures CPU throughput, memory bandwidth, and cache latency.
 *
 * Compile: gcc -O2 -msse2 -o hwbench hwbench.c
 * Run:     ./hwbench [--size MB]
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <emmintrin.h>
#include <unistd.h>
#include <sched.h>
#include <sys/sysinfo.h>

#define RESET   "\033[0m"
#define BOLD    "\033[1m"
#define CYAN    "\033[36m"
#define GREEN   "\033[32m"

static size_t mem_bytes = 256UL * 1024 * 1024;
static uint64_t lcg_state = 0x5EED;

static inline uint32_t fast_rand(void) {
    lcg_state = lcg_state * 1103515245 + 12345;
    return (uint32_t)(lcg_state >> 16);
}

static double elapsed_ms(struct timespec *a, struct timespec *b) {
    return (b->tv_sec - a->tv_sec) * 1000.0
         + (b->tv_nsec - a->tv_nsec) / 1e6;
}

/*
 * CPU integer 32-bit: LCG chain. Each iteration depends on the previous result,
 * preventing the loop from being parallelized or hoisted by the compiler.
 */
static double bench_cpu_int32(void) {
    const uint64_t iters = 1000000000ULL;
    uint32_t x = 1;
    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint64_t i = 0; i < iters; i++)
        x = x * 1664525 + 1013904223;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    if (x == 0) putchar(0);
    return (double)iters / elapsed_ms(&t0, &t1) / 1e3;
}

/*
 * CPU integer 64-bit: LCG chain with 64-bit multiply.
 */
static double bench_cpu_int64(void) {
    const uint64_t iters = 1000000000ULL;
    uint64_t x = 1;
    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint64_t i = 0; i < iters; i++)
        x = x * 6364136223846793005ULL + 1442695040888963407ULL;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    if (x == 0) putchar(0);
    return (double)iters / elapsed_ms(&t0, &t1) / 1e3;
}

/*
 * CPU float: single-precision dependent multiply-add chain.
 */
static double bench_cpu_float32(void) {
    const uint64_t iters = 500000000ULL;
    float x = 1.0f;
    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint64_t i = 0; i < iters; i++)
        x = x * 1.0000001f + 0.0000003f;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    if (x == 0.0f) putchar(0);
    return (double)iters / elapsed_ms(&t0, &t1) / 1e3;
}

/*
 * CPU double: double-precision dependent multiply-add chain.
 */
static double bench_cpu_float64(void) {
    const uint64_t iters = 500000000ULL;
    double x = 1.0;
    struct timespec t0, t1;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (uint64_t i = 0; i < iters; i++)
        x = x * 1.0000001 + 0.0000003;
    clock_gettime(CLOCK_MONOTONIC, &t1);

    if (x == 0.0) putchar(0);
    return (double)iters / elapsed_ms(&t0, &t1) / 1e3;
}

/*
 * Memory bandwidth: two write paths + one read path.
 *
 * memset write  — goes through cache (write-allocate). Each store triggers
 *                 a Read-For-Ownership (RFO): the CPU fetches the cache line
 *                 before writing it, doubling actual memory traffic. This is
 *                 the culprit when write bandwidth appears far below read.
 *
 * NT write      — non-temporal stores (MOVNTDQ via SSE2). Bypasses cache
 *                 entirely: no RFO, no eviction traffic. Writes stream
 *                 directly to memory. This is the true write bandwidth.
 *
 * read          — sequential load + accumulate. Compiler vectorizes with
 *                 SIMD loads; volatile sink prevents elision.
 *
 * Result in GB/s (1 GB = 1e9 bytes).
 */
static void bench_memory(double *wr_cached, double *wr_nt, double *rd) {
    /* 16-byte alignment required for MOVNTDQ */
    uint8_t *buf = aligned_alloc(16, mem_bytes);
    if (!buf) { *wr_cached = *wr_nt = *rd = 0.0; return; }

    memset(buf, 0xAB, mem_bytes); /* fault in all pages before timing */

    struct timespec t0, t1;

    /* write via memset (write-allocate, RFO path) */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    memset(buf, 0xCD, mem_bytes);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    *wr_cached = (double)mem_bytes / elapsed_ms(&t0, &t1) / 1e6;

    /* write via non-temporal stores (bypass cache, no RFO) */
    __m128i val = _mm_set1_epi8(0xEF);
    __m128i *dst = (__m128i *)buf;
    size_t chunks = mem_bytes / 16;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < chunks; i++)
        _mm_stream_si128(dst + i, val);
    _mm_sfence(); /* ensure stores complete before timing */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    *wr_nt = (double)mem_bytes / elapsed_ms(&t0, &t1) / 1e6;

    /* sequential read */
    uint64_t sum = 0;
    uint64_t *p = (uint64_t *)buf;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < mem_bytes / 8; i++) sum += p[i];
    clock_gettime(CLOCK_MONOTONIC, &t1);
    *rd = (double)mem_bytes / elapsed_ms(&t0, &t1) / 1e6;

    volatile uint64_t sink = sum; (void)sink;
    free(buf);
}

/*
 * Cache latency: random pointer chase. A shuffled index array creates a
 * single dependent load chain — each access address is the result of the
 * previous load, so the CPU cannot prefetch or overlap iterations.
 * Returns nanoseconds per access.
 */
static double bench_cache(size_t bytes, size_t steps) {
    size_t n = bytes / sizeof(size_t);
    if (n < 2) n = 2;
    size_t *arr = malloc(n * sizeof(size_t));
    if (!arr) return 0.0;

    for (size_t i = 0; i < n; i++) arr[i] = i;

    uint32_t rstate = 0x5EED;
    for (size_t i = n - 1; i > 0; i--) {
        uint32_t r = (rstate = rstate * 1664525 + 1013904223) >> 16;
        size_t j = r % (i + 1);
        size_t t = arr[i]; arr[i] = arr[j]; arr[j] = t;
    }

    volatile size_t idx = 0;
    for (size_t i = 0; i < 100000; i++) idx = arr[idx];

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (size_t i = 0; i < steps; i++) idx = arr[idx];
    clock_gettime(CLOCK_MONOTONIC, &t1);

    (void)idx;
    free(arr);
    return elapsed_ms(&t0, &t1) * 1e6 / (double)steps;
}

static void bar(double val, double ref, int width) {
    int filled = (int)(val / ref * width);
    if (filled > width) filled = width;
    printf(GREEN "[");
    for (int i = 0; i < width; i++) putchar(i < filled ? '#' : ' ');
    printf("]" RESET);
}

static void pin_to_cpu(void) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) != 0) {
        fprintf(stderr, "Warning: could not pin to CPU 0\n");
    }
}

static void usage(const char *prog) {
    printf("Usage: %s [--size MB]\n", prog);
    printf("  --size MB   Memory buffer size in MB (default: 256)\n");
}

static void parse_args(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            long mb = strtol(argv[++i], NULL, 10);
            if (mb > 0 && mb <= 8192) {
                mem_bytes = (size_t)mb * 1024 * 1024;
            } else {
                fprintf(stderr, "Invalid size, using default 256MB\n");
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(0);
        }
    }
}

int main(int argc, char *argv[]) {
    parse_args(argc, argv);
    pin_to_cpu();

    printf(BOLD "\nhwbench — Hardware Benchmark\n" RESET);
    printf("────────────────────────────────────────────\n");
    printf("Memory buffer: %.0f MB\n", (double)mem_bytes / 1024 / 1024);

    /* CPU */
    printf(CYAN "\nCPU\n" RESET);

    printf("  Integer (32-bit)       : ");
    fflush(stdout);
    double ci32 = bench_cpu_int32();
    printf(BOLD "%6.0f MOPS  " RESET, ci32);
    bar(ci32, 5000.0, 24);
    putchar('\n');

    printf("  Integer (64-bit)       : ");
    fflush(stdout);
    double ci64 = bench_cpu_int64();
    printf(BOLD "%6.0f MOPS  " RESET, ci64);
    bar(ci64, 5000.0, 24);
    putchar('\n');

    printf("  Float (single)         : ");
    fflush(stdout);
    double cf32 = bench_cpu_float32();
    printf(BOLD "%6.0f MOPS  " RESET, cf32);
    bar(cf32, 5000.0, 24);
    putchar('\n');

    printf("  Double precision       : ");
    fflush(stdout);
    double cf64 = bench_cpu_float64();
    printf(BOLD "%6.0f MOPS  " RESET, cf64);
    bar(cf64, 5000.0, 24);
    putchar('\n');

    /* Memory */
    printf(CYAN "\nMemory Bandwidth\n" RESET);

    double mw, mw_nt, mr;
    bench_memory(&mw, &mw_nt, &mr);

    printf("  Write (cached/RFO)    : ");
    printf(BOLD "%6.1f GB/s  " RESET, mw);
    bar(mw, 50.0, 24);
    putchar('\n');

    printf("  Write (non-temporal)  : ");
    printf(BOLD "%6.1f GB/s  " RESET, mw_nt);
    bar(mw_nt, 50.0, 24);
    putchar('\n');

    printf("  Sequential read       : ");
    printf(BOLD "%6.1f GB/s  " RESET, mr);
    bar(mr, 50.0, 24);
    putchar('\n');

    /* Cache latency */
    printf(CYAN "\nCache Latency\n" RESET);

    static const struct {
        const char *label;
        size_t      bytes;
        size_t      steps;
    } levels[] = {
        { "16 KB   (L1)", 16UL  * 1024,        50000000 },
        { "256 KB  (L2)", 256UL * 1024,         20000000 },
        { "4 MB    (L3)", 4UL   * 1024 * 1024,  10000000 },
        { "256 MB  (RAM)",256UL * 1024 * 1024,   2000000 },
    };

    for (int i = 0; i < 4; i++) {
        printf("  %-22s: ", levels[i].label);
        fflush(stdout);
        double ns = bench_cache(levels[i].bytes, levels[i].steps);
        printf(BOLD "%6.1f ns  " RESET, ns);
        bar(ns, 200.0, 24);
        putchar('\n');
    }

    printf("\n────────────────────────────────────────────\n\n");
    return 0;
}
