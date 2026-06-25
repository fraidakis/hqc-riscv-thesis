/* tests/bench/roofline_peak.c
 *
 * Roofline micro-benchmarks for the SpacemiT X60 (BPI-F3).
 *
 * Measures three ceilings:
 *   1. Peak scalar compute (clmul-bound loop) — ops/s
 *   2. Peak vector compute (RVV arithmetic-bound loop) — ops/s
 *   3. Peak DRAM bandwidth (STREAM-like copy, sized beyond LLC) — bytes/s
 *
 * Operation unit definition (for roofline consistency):
 *   "Useful arithmetic operations" = retired arithmetic instructions relevant
 *   to the HQC polynomial multiply kernel.
 *   - For scalar_zb: clmul/clmulh instructions
 *   - For vector_rvv: RVV arithmetic vector instructions (vclmul, vxor, etc.)
 *   - Fallback: retired instructions (instret)
 *   The operation unit is INSTRUCTIONS (not FLOPs — this is integer/GF(2) crypto).
 *
 * Output (CSV to stdout):
 *   ceiling,value,unit
 *   peak_compute_scalar,<ops_per_sec>,ops/s
 *   peak_compute_vector,<ops_per_sec>,ops/s
 *   peak_dram_bw,<bytes_per_sec>,bytes/s
 *
 * Compile:
 *   gcc -O3 -static -march=rv64gcv_zbc_zbb_zbs -o roofline_peak roofline_peak.c
 *
 * Run (on core 7, performance governor):
 *   sudo nice -n -20 taskset -c 7 ./roofline_peak
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ── Timing helper ───────────────────────────────────────────────────────── */
static inline uint64_t rdcycle(void)
{
    uint64_t val;
    __asm__ volatile("rdcycle %0" : "=r"(val));
    return val;
}

static inline double get_time_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Ceiling 1: Peak scalar compute — clmul throughput
 *
 * The X60 has 1 clmul unit. We saturate it with independent clmul chains
 * (no data dependencies between iterations). This gives the maximum
 * clmul operations per second achievable by the scalar_zb variant.
 * ══════════════════════════════════════════════════════════════════════════ */

#define SCALAR_ITERS (100 * 1000 * 1000ULL)  /* 100M iterations */
#define CLMUL_OPS_PER_ITER 4  /* 4 independent clmul per loop body */

static uint64_t __attribute__((noinline)) bench_peak_scalar(void)
{
    register uint64_t a0 = 0xDEADBEEF12345678ULL;
    register uint64_t a1 = 0xCAFEBABE87654321ULL;
    register uint64_t a2 = 0x1234567890ABCDEFULL;
    register uint64_t a3 = 0xFEDCBA0987654321ULL;
    register uint64_t b  = 0xA5A5A5A5A5A5A5A5ULL;

    for (uint64_t i = 0; i < SCALAR_ITERS; i++) {
        __asm__ volatile(
            "clmul %0, %0, %4\n\t"
            "clmul %1, %1, %4\n\t"
            "clmul %2, %2, %4\n\t"
            "clmul %3, %3, %4\n\t"
            : "+r"(a0), "+r"(a1), "+r"(a2), "+r"(a3)
            : "r"(b)
        );
    }

    /* Anti-DCE */
    return a0 ^ a1 ^ a2 ^ a3;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Ceiling 2: Peak vector compute — RVV arithmetic throughput
 *
 * Saturate the VPU with independent vector XOR operations (widest LMUL).
 * On X60: VLEN=256, LMUL=8 → 2048-bit vectors, ~1 vxor/cycle at best.
 * This represents the vector arithmetic throughput ceiling for vector_rvv.
 * ══════════════════════════════════════════════════════════════════════════ */

#define VECTOR_ITERS (50 * 1000 * 1000ULL)  /* 50M iterations */
#define VEC_OPS_PER_ITER 4  /* 4 independent vector ops per loop body */

static uint64_t __attribute__((noinline)) bench_peak_vector(void)
{
    /* Use LMUL=8 for maximum throughput (processes 256 bytes per vxor) */
    uint64_t dummy = 0;

    __asm__ volatile(
        /* Set vl to max for e64, m8 */
        "vsetvli t0, zero, e64, m8, ta, ma\n\t"
        /* Initialize vector registers with non-zero data */
        "li t1, 0xA5A5A5A5\n\t"
        "vmv.v.x v0, t1\n\t"
        "vmv.v.x v8, t1\n\t"
        "vmv.v.x v16, t1\n\t"
        "vmv.v.x v24, t1\n\t"
        ::: "t0", "t1", "memory"
    );

    for (uint64_t i = 0; i < VECTOR_ITERS; i++) {
        __asm__ volatile(
            "vxor.vv v0, v0, v8\n\t"
            "vxor.vv v8, v8, v16\n\t"
            "vxor.vv v16, v16, v24\n\t"
            "vxor.vv v24, v24, v0\n\t"
            ::: "memory"
        );
    }

    /* Anti-DCE: move one element out */
    __asm__ volatile(
        "vmv.x.s %0, v0\n\t"
        : "=r"(dummy)
    );

    return dummy;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Ceiling 3: Peak DRAM bandwidth — STREAM-like copy
 *
 * Array size well beyond LLC (X60: 512KB L2 per core-pair, no shared L3).
 * We use 16 MB arrays to ensure we're measuring main memory bandwidth.
 * STREAM Copy: c[i] = a[i] (1 read + 1 write per element).
 * ══════════════════════════════════════════════════════════════════════════ */

#define STREAM_SIZE (16 * 1024 * 1024 / sizeof(uint64_t))  /* 16 MB */
#define STREAM_REPS 5  /* multiple reps, take best (least perturbed) */

static uint64_t *stream_a;
static uint64_t *stream_c;

static double bench_peak_dram_bw(void)
{
    /* Initialize arrays (also pre-fault pages) */
    for (size_t i = 0; i < STREAM_SIZE; i++) {
        stream_a[i] = (uint64_t)i;
        stream_c[i] = 0;
    }

    double best_bw = 0.0;

    for (int rep = 0; rep < STREAM_REPS; rep++) {
        double t0 = get_time_ns();

        /* STREAM Copy: c[i] = a[i] */
        for (size_t i = 0; i < STREAM_SIZE; i++) {
            stream_c[i] = stream_a[i];
        }

        double t1 = get_time_ns();
        double elapsed_s = (t1 - t0) / 1e9;

        /* Bytes transferred: read + write = 2 × array_size */
        double bytes = 2.0 * (double)STREAM_SIZE * sizeof(uint64_t);
        double bw = bytes / elapsed_s;

        if (bw > best_bw) best_bw = bw;
    }

    /* Anti-DCE */
    volatile uint64_t sink = stream_c[STREAM_SIZE / 2];
    (void)sink;

    return best_bw;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(void)
{
    /* Allocate STREAM arrays */
    stream_a = (uint64_t *)malloc(STREAM_SIZE * sizeof(uint64_t));
    stream_c = (uint64_t *)malloc(STREAM_SIZE * sizeof(uint64_t));
    if (!stream_a || !stream_c) {
        fprintf(stderr, "ERROR: cannot allocate STREAM arrays (32 MB)\n");
        return 1;
    }

    fprintf(stderr, "Measuring peak ceilings for roofline model...\n");

    /* ── Scalar peak ──────────────────────────────────────────────────── */
    fprintf(stderr, "  [1/3] Peak scalar compute (clmul throughput)...\n");
    double t0 = get_time_ns();
    uint64_t scalar_sink = bench_peak_scalar();
    double t1 = get_time_ns();
    double scalar_elapsed_s = (t1 - t0) / 1e9;
    double scalar_ops = (double)SCALAR_ITERS * CLMUL_OPS_PER_ITER;
    double scalar_ops_per_s = scalar_ops / scalar_elapsed_s;

    /* ── Vector peak ──────────────────────────────────────────────────── */
    fprintf(stderr, "  [2/3] Peak vector compute (RVV vxor throughput)...\n");
    t0 = get_time_ns();
    uint64_t vector_sink = bench_peak_vector();
    t1 = get_time_ns();
    double vector_elapsed_s = (t1 - t0) / 1e9;
    /* Each vxor with LMUL=8 processes VLEN*8/64 = 32 elements at once.
     * But for the roofline, we count VECTOR INSTRUCTIONS, not elements. */
    double vector_ops = (double)VECTOR_ITERS * VEC_OPS_PER_ITER;
    double vector_ops_per_s = vector_ops / vector_elapsed_s;

    /* ── DRAM bandwidth ───────────────────────────────────────────────── */
    fprintf(stderr, "  [3/3] Peak DRAM bandwidth (STREAM copy)...\n");
    double dram_bw = bench_peak_dram_bw();

    /* ── Output CSV ───────────────────────────────────────────────────── */
    printf("ceiling,value,unit\n");
    printf("peak_compute_scalar,%.0f,ops/s\n", scalar_ops_per_s);
    printf("peak_compute_vector,%.0f,ops/s\n", vector_ops_per_s);
    printf("peak_dram_bw,%.0f,bytes/s\n", dram_bw);

    /* Anti-DCE */
    volatile uint64_t final_sink = scalar_sink ^ vector_sink;
    (void)final_sink;

    free(stream_a);
    free(stream_c);

    fprintf(stderr, "Done.\n");
    fprintf(stderr, "  Scalar: %.2f Gops/s (clmul)\n", scalar_ops_per_s / 1e9);
    fprintf(stderr, "  Vector: %.2f Gops/s (vxor m8)\n", vector_ops_per_s / 1e9);
    fprintf(stderr, "  DRAM:   %.2f GB/s (copy)\n", dram_bw / 1e9);

    return 0;
}
