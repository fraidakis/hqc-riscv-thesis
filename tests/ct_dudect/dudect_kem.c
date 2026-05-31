/* tests/ct_dudect/dudect_kem.c
 *
 * Dudect constant-time verification harness for HQC KEM decapsulation.
 *
 * Implements the dudect methodology (Reparaz et al., 2016):
 *   - Two input classes: fixed (class A) vs random (class B) (sk, ct) pairs
 *   - Timing via perf_event_open (hardware cycle counters)
 *   - Streaming Welch's t-test (first-order + second-order percentile-cropped)
 *   - Auto-termination: LEAKAGE if |t| > T_THRESHOLD; PASS if n >= N_MAX
 *   - CSV output for thesis convergence plots
 *
 * Measurement methodology (prepare-then-measure):
 *   Phase 1 — Prepare: Generate ALL (sk, ct) pairs for the batch.
 *             Class A copies the fixed pair; Class B generates fresh keypair+enc.
 *             Both classes write to the same per-measurement buffer slots.
 *   Phase 2 — Measure: Tight loop that times only crypto_kem_dec().
 *             No key generation, no class-dependent branching inside the
 *             timed region.
 *
 * This two-phase design eliminates cache-state asymmetry caused by
 * keypair+encaps computation warming caches differently for Class B.
 *
 * Measured function: crypto_kem_dec(ss, ct, sk)
 *
 * Usage:
 *   ./dudect_kem_hqc128_karatsuba_zbc [--outdir /path/to/results]
 *
 * Build with Makefile.hw:
 *   make -f Makefile.hw TARGET=kem LEVEL=128 POLYMUL=karatsuba_zbc
 *
 * Requirements: 2.1, 2.2, 2.3, 2.4, 3.1, 3.2, 3.3, 3.4, 5.1, 5.2,
 *               6.1, 6.2, 6.3, 6.4, 7.1, 7.2, 7.3, 8.1, 8.2, 8.3, 8.4,
 *               9.1, 9.2, 9.3, 9.4, 10.1, 10.2, 10.3, 12.2, 12.3,
 *               15.1, 15.2, 15.3
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* HQC KEM API and internals */
#include "api.h"
#include "parameters.h"
#include "symmetric.h"

/* Dudect infrastructure */
#include "perf_hw.h"
#include "dudect_ttest.h"
#include "dudect_percentile.h"
#include "dudect_prng.h"

/* ── Compile-time configuration ──────────────────────────────────────────── */

#ifndef N_MAX
#  define N_MAX 100000
#endif

#ifndef BATCH_SIZE
#  define BATCH_SIZE 5000
#endif

#ifndef T_THRESHOLD
#  define T_THRESHOLD 4.5
#endif

#ifndef PERCENTILE_CROP
#  define PERCENTILE_CROP 95
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

#ifndef SECURITY_LEVEL
#  define SECURITY_LEVEL 128
#endif

/* ── Exit codes ──────────────────────────────────────────────────────────── */

#define EXIT_PASS     0
#define EXIT_LEAKAGE  1
#define EXIT_ERROR    2

/* ── CSV Reporter ────────────────────────────────────────────────────────── */

typedef struct {
    FILE *fp;
    int batch_num;
} csv_reporter_t;

static int csv_reporter_open(csv_reporter_t *r, const char *path)
{
    r->fp = fopen(path, "w");
    if (!r->fp) {
        fprintf(stderr, "[dudect] ERROR: cannot open CSV file: %s (%s)\n",
                path, strerror(errno));
        return -1;
    }
    fprintf(r->fp, "batch,n_measurements,t_statistic_first_order,t_statistic_second_order\n");
    fflush(r->fp);
    r->batch_num = 0;
    return 0;
}

static void csv_reporter_write_batch(csv_reporter_t *r,
                                     int64_t n_total,
                                     double t_first, double t_second)
{
    r->batch_num++;
    fprintf(r->fp, "%d,%ld,%.6f,%.6f\n",
            r->batch_num, (long)n_total, t_first, t_second);
    fflush(r->fp);
}

static void csv_reporter_close(csv_reporter_t *r)
{
    if (r->fp) {
        fclose(r->fp);
        r->fp = NULL;
    }
}

/* ── Dudect state ────────────────────────────────────────────────────────── */

typedef struct {
    /* Statistical engines */
    ttest_ctx_t ttest_first_order;
    ttest_ctx_t ttest_second_order;

    /* PRNG */
    dudect_prng_t prng;

    /* Measurement infrastructure */
    perf_hw_ctx perf;

    /* Batch buffers (reused each batch) */
    uint64_t *batch_times;
    int      *batch_classes;

    /* Pre-allocated input buffers for prepare-then-measure pattern.
     * Each measurement i reads from input_sk[i] and input_ct[i]. */
    uint8_t *input_sk;   /* BATCH_SIZE * CRYPTO_SECRETKEYBYTES */
    uint8_t *input_ct;   /* BATCH_SIZE * CRYPTO_CIPHERTEXTBYTES */

    /* Cumulative state */
    int64_t total_measurements;
    int     batch_count;

    /* Output */
    csv_reporter_t csv;
} dudect_state_t;

/* ── Class A: fixed (sk, pk, ct) generated once at init (Req 2.2) ────────── */
static uint8_t fixed_pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t fixed_sk[CRYPTO_SECRETKEYBYTES];
static uint8_t fixed_ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t fixed_ss_enc[CRYPTO_BYTES];

/* Decapsulation output buffer */
static uint8_t dec_ss[CRYPTO_BYTES];

/* ── Directory creation (mkdir -p equivalent) ────────────────────────────── */

static int mkdirp(const char *path)
{
    char tmp[512];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (len > 0 && tmp[len - 1] == '/')
        tmp[len - 1] = '\0';

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
        return -1;
    return 0;
}

/* ── Startup checks ─────────────────────────────────────────────────────── */

static void check_governor(void)
{
    FILE *f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (f) {
        char gov[64] = {0};
        if (fgets(gov, sizeof(gov), f)) {
            char *nl = strchr(gov, '\n');
            if (nl) *nl = '\0';
            if (strcmp(gov, "performance") != 0) {
                fprintf(stderr, "[dudect] WARNING: CPU governor is '%s', not 'performance'.\n"
                        "  Run: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor\n",
                        gov);
            } else {
                fprintf(stderr, "[dudect] CPU governor: performance ✓\n");
            }
        }
        fclose(f);
    } else {
        fprintf(stderr, "[dudect] WARNING: cannot read CPU governor (file not found)\n");
    }
}

static int validate_perf_counters(void)
{
    perf_hw_ctx test_ctx;
    if (perf_hw_init(&test_ctx) != 0) {
        fprintf(stderr, "[dudect] ERROR: perf_event_open failed. "
                "Check /proc/sys/kernel/perf_event_paranoid\n");
        return -1;
    }

    perf_hw_start(&test_ctx);
    volatile int x = 0;
    for (int i = 0; i < 1000; i++) x += i;
    perf_hw_stop(&test_ctx);

    perf_hw_sample s;
    perf_hw_read(&test_ctx, &s);
    perf_hw_close(&test_ctx);

    (void)x;

    if (s.cycles == 0) {
        fprintf(stderr, "[dudect] ERROR: perf counters return 0 cycles. "
                "Check /proc/sys/kernel/perf_event_paranoid\n");
        return -1;
    }

    fprintf(stderr, "[dudect] perf counters validated (test: %lu cycles)\n",
            (unsigned long)s.cycles);
    return 0;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    /* Default output directory */
    const char *outdir = "results/ct_verify/dudect";

    /* Parse --outdir argument (Req 15.3) */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--outdir") == 0 && i + 1 < argc) {
            outdir = argv[i + 1];
            i++;
        }
    }

    /* ── Print target info ────────────────────────────────────────────── */
    fprintf(stderr, "[dudect] target=crypto_kem_dec variant=%s level=%d\n",
            POLYMUL_NAME, SECURITY_LEVEL);
    fprintf(stderr, "[dudect] N_MAX=%d BATCH_SIZE=%d T_THRESHOLD=%.1f PERCENTILE=%d\n",
            N_MAX, BATCH_SIZE, T_THRESHOLD, PERCENTILE_CROP);

    /* ── Startup checks (Req 12.2, 12.3) ─────────────────────────────── */
    check_governor();
    if (validate_perf_counters() != 0)
        return EXIT_ERROR;

    /* ── Initialize state ─────────────────────────────────────────────── */
    dudect_state_t state;
    memset(&state, 0, sizeof(state));

    ttest_init(&state.ttest_first_order);
    ttest_init(&state.ttest_second_order);

    /* ── Initialize PRNG (Req 16.1, 16.2, 16.3) ──────────────────────── */
    if (dudect_prng_init(&state.prng) != 0)
        return EXIT_ERROR;

    /* ── Seed the HQC internal PRNG (SHAKE-256 based) ─────────────────── */
    static const uint8_t dummy_seed[48] = { 0 };
    prng_init((uint8_t *)dummy_seed, NULL, 48, 0);

    /* ── Generate Class A inputs: fixed keypair + encaps (Req 2.2) ────── */
    crypto_kem_keypair(fixed_pk, fixed_sk);
    crypto_kem_enc(fixed_ct, fixed_ss_enc, fixed_pk);
    fprintf(stderr, "[dudect] Class A: fixed (sk, pk, ct) generated\n");

    /* ── Initialize perf counters (Req 3.1, 3.4) ─────────────────────── */
    if (perf_hw_init(&state.perf) != 0) {
        fprintf(stderr, "[dudect] ERROR: cannot initialize perf counters\n");
        return EXIT_ERROR;
    }

    /* ── Allocate batch buffers (Req 8.1) ─────────────────────────────── */
    state.batch_times = (uint64_t *)malloc((size_t)BATCH_SIZE * sizeof(uint64_t));
    state.batch_classes = (int *)malloc((size_t)BATCH_SIZE * sizeof(int));

    /* Allocate input buffers for prepare-then-measure pattern */
    state.input_sk = (uint8_t *)malloc((size_t)BATCH_SIZE * CRYPTO_SECRETKEYBYTES);
    state.input_ct = (uint8_t *)malloc((size_t)BATCH_SIZE * CRYPTO_CIPHERTEXTBYTES);

    if (!state.batch_times || !state.batch_classes ||
        !state.input_sk || !state.input_ct) {
        fprintf(stderr, "[dudect] ERROR: failed to allocate batch buffers\n");
        return EXIT_ERROR;
    }

    fprintf(stderr, "[dudect] input buffers: sk=%zu MB, ct=%zu MB\n",
            ((size_t)BATCH_SIZE * CRYPTO_SECRETKEYBYTES) / (1024*1024),
            ((size_t)BATCH_SIZE * CRYPTO_CIPHERTEXTBYTES) / (1024*1024));

    /* ── Create output directory (Req 15.1) ───────────────────────────── */
    if (mkdirp(outdir) != 0) {
        fprintf(stderr, "[dudect] ERROR: cannot create output directory: %s (%s)\n",
                outdir, strerror(errno));
        return EXIT_ERROR;
    }

    /* ── Open CSV reporter (Req 9.1, 9.2) ─────────────────────────────── */
    char csv_path[512];
    snprintf(csv_path, sizeof(csv_path), "%s/dudect_kem_hqc%d_%s.csv",
             outdir, SECURITY_LEVEL, POLYMUL_NAME);

    if (csv_reporter_open(&state.csv, csv_path) != 0)
        return EXIT_ERROR;

    fprintf(stderr, "[dudect] CSV output: %s\n", csv_path);

    /* ── Temporary buffers for Class B key generation ─────────────────── */
    uint8_t tmp_pk[CRYPTO_PUBLICKEYBYTES];
    uint8_t tmp_ss[CRYPTO_BYTES];

    /* ── Warm-up round (Req 8.4) ──────────────────────────────────────── *
     * Run one full batch to warm up caches, TLB, branch predictor, and
     * page tables. Timing measurements are discarded — not fed into the
     * t-test accumulators. This eliminates cold-start transients that
     * would otherwise bias early batches asymmetrically.              */
    {
        for (int i = 0; i < BATCH_SIZE; i++) {
            int class_id = dudect_prng_bit(&state.prng);
            uint8_t *dst_sk = state.input_sk + (size_t)i * CRYPTO_SECRETKEYBYTES;
            uint8_t *dst_ct = state.input_ct + (size_t)i * CRYPTO_CIPHERTEXTBYTES;
            if (class_id == 0) {
                memcpy(dst_sk, fixed_sk, CRYPTO_SECRETKEYBYTES);
                memcpy(dst_ct, fixed_ct, CRYPTO_CIPHERTEXTBYTES);
            } else {
                crypto_kem_keypair(tmp_pk, dst_sk);
                crypto_kem_enc(dst_ct, tmp_ss, tmp_pk);
            }
        }
        for (int i = 0; i < BATCH_SIZE; i++) {
            uint8_t *sk_ptr = state.input_sk + (size_t)i * CRYPTO_SECRETKEYBYTES;
            uint8_t *ct_ptr = state.input_ct + (size_t)i * CRYPTO_CIPHERTEXTBYTES;
            perf_hw_sample sample;
            perf_hw_start(&state.perf);
            crypto_kem_dec(dec_ss, ct_ptr, sk_ptr);
            perf_hw_stop(&state.perf);
            perf_hw_read(&state.perf, &sample);
            __asm__ volatile("" : : "r"(dec_ss) : "memory");
        }
        fprintf(stderr, "[dudect] warm-up batch completed (discarded)\n");
    }

    /* ── Main measurement loop ────────────────────────────────────────── */
    int verdict = -1; /* -1 = continue, 0 = pass, 1 = leakage */

    while (verdict < 0) {
        /* ══════════════════════════════════════════════════════════════════
         * PHASE 1: PREPARE — Generate all (sk, ct) pairs for this batch.
         *
         * Both classes write to the same input_sk/input_ct buffer array.
         * Class A copies fixed_{sk,ct}; Class B generates fresh pairs.
         * ══════════════════════════════════════════════════════════════════ */
        for (int i = 0; i < BATCH_SIZE; i++) {
            int class_id = dudect_prng_bit(&state.prng);
            state.batch_classes[i] = class_id;

            uint8_t *dst_sk = state.input_sk + (size_t)i * CRYPTO_SECRETKEYBYTES;
            uint8_t *dst_ct = state.input_ct + (size_t)i * CRYPTO_CIPHERTEXTBYTES;

            if (class_id == 0) {
                /* Class A: copy fixed (sk, ct) into per-measurement slot */
                memcpy(dst_sk, fixed_sk, CRYPTO_SECRETKEYBYTES);
                memcpy(dst_ct, fixed_ct, CRYPTO_CIPHERTEXTBYTES);
            } else {
                /* Class B: generate fresh keypair + encaps */
                crypto_kem_keypair(tmp_pk, dst_sk);
                crypto_kem_enc(dst_ct, tmp_ss, tmp_pk);
            }
        }

        /* ══════════════════════════════════════════════════════════════════
         * PHASE 2: MEASURE — Tight timing loop.
         *
         * No key generation, no class-dependent branching. Each iteration
         * reads from pre-filled input_sk[i], input_ct[i] and times only
         * crypto_kem_dec().
         * ══════════════════════════════════════════════════════════════════ */
        for (int i = 0; i < BATCH_SIZE; i++) {
            uint8_t *sk_ptr = state.input_sk + (size_t)i * CRYPTO_SECRETKEYBYTES;
            uint8_t *ct_ptr = state.input_ct + (size_t)i * CRYPTO_CIPHERTEXTBYTES;

            perf_hw_sample sample;
            perf_hw_start(&state.perf);
            crypto_kem_dec(dec_ss, ct_ptr, sk_ptr);
            perf_hw_stop(&state.perf);
            perf_hw_read(&state.perf, &sample);

            /* Anti-DCE barrier */
            __asm__ volatile("" : : "r"(dec_ss) : "memory");

            state.batch_times[i] = sample.cycles;
        }

        state.total_measurements += BATCH_SIZE;
        state.batch_count++;

        /* ── Feed measurements to first-order t-test (Req 5.1) ────────── */
        for (int i = 0; i < BATCH_SIZE; i++) {
            ttest_push(&state.ttest_first_order,
                       (double)state.batch_times[i],
                       state.batch_classes[i]);
        }

        /* ── Second-order: percentile cropping (Req 6.1, 6.2, 6.3, 6.4) ── */
        uint64_t *sorted_times = (uint64_t *)malloc((size_t)BATCH_SIZE * sizeof(uint64_t));
        if (sorted_times) {
            memcpy(sorted_times, state.batch_times, (size_t)BATCH_SIZE * sizeof(uint64_t));
            uint64_t threshold = percentile_threshold(sorted_times, BATCH_SIZE, PERCENTILE_CROP);
            free(sorted_times);

            for (int i = 0; i < BATCH_SIZE; i++) {
                if (state.batch_times[i] <= threshold) {
                    ttest_push(&state.ttest_second_order,
                               (double)state.batch_times[i],
                               state.batch_classes[i]);
                }
            }
        }

        /* ── Compute t-statistics (Req 5.2, 8.2) ─────────────────────── */
        double t_first = ttest_compute(&state.ttest_first_order);
        double t_second = ttest_compute(&state.ttest_second_order);

        /* ── Write CSV row (Req 9.3, 9.4) ─────────────────────────────── */
        csv_reporter_write_batch(&state.csv, state.total_measurements,
                                 t_first, t_second);

        /* ── Progress output (Req 10.3) ───────────────────────────────── */
        fprintf(stderr, "[dudect] batch %3d: n=%7ld  t1=%7.3f  t2=%7.3f\n",
                state.batch_count, (long)state.total_measurements,
                t_first, t_second);

        /* ── Termination check (Req 7.1, 7.2, 7.3) ───────────────────── */
        if (fabs(t_first) > T_THRESHOLD || fabs(t_second) > T_THRESHOLD) {
            verdict = 1; /* LEAKAGE */
        } else if (state.total_measurements >= N_MAX) {
            verdict = 0; /* PASS */
        }
    }

    /* ── Summary output (Req 10.1, 10.2) ──────────────────────────────── */
    double t_first_final = ttest_compute(&state.ttest_first_order);
    double t_second_final = ttest_compute(&state.ttest_second_order);

    if (verdict == 1) {
        fprintf(stderr, "\n[dudect] RESULT: LEAKAGE DETECTED "
                "(target=crypto_kem_dec variant=%s level=%d n=%ld t1=%.3f t2=%.3f)\n",
                POLYMUL_NAME, SECURITY_LEVEL,
                (long)state.total_measurements, t_first_final, t_second_final);
    } else {
        fprintf(stderr, "\n[dudect] RESULT: no leakage detected "
                "(target=crypto_kem_dec variant=%s level=%d n=%ld t1=%.3f t2=%.3f)\n",
                POLYMUL_NAME, SECURITY_LEVEL,
                (long)state.total_measurements, t_first_final, t_second_final);
    }

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    csv_reporter_close(&state.csv);
    perf_hw_close(&state.perf);
    free(state.batch_times);
    free(state.batch_classes);
    free(state.input_sk);
    free(state.input_ct);

    return (verdict == 1) ? EXIT_LEAKAGE : EXIT_PASS;
}
