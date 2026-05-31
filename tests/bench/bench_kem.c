/* tests/bench/bench_kem.c
 *
 * Unified KEM benchmark for HQC — works for both bare-metal simulation
 * (Spike ISS) and real hardware (BPI-F3 / Linux userspace).
 *
 * Platform selection:
 *   - Without HQC_PLATFORM_HW: uses bare-metal rdcycle/rdinstret via bench.h
 *   - With -DHQC_PLATFORM_HW=1: uses perf_event_open via perf_hw.h
 *
 * Simulation output format (single-run, instruction-accurate):
 *   # level=<L>,polymul=<P>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   keypair,1,...
 *   encaps,1,...
 *   decaps,1,...
 *
 * Hardware output format (multi-iteration, statistical):
 *   # level=<L>,polymul=<P>,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   keypair,10000,...
 *   encaps,10000,...
 *   decaps,10000,...
 *
 * Compile-time knobs (set by CMakeLists.txt or Makefile.hw):
 *   SECURITY_LEVEL    128 | 192 | 256
 *   POLYMUL_NAME      string literal, e.g. "karatsuba_zbc"
 *   HQC_BENCH_MODE=1  ensures AES-free randombytes()
 *   N_ITER            (optional, overrides N_ITER_DEFAULT)
 *   N_WARMUP          (optional, overrides N_WARMUP_DEFAULT)
 *   HQC_SILENT        suppresses HTIF metadata headers (sim only)
 *   HQC_TRACE_MODE    compiles out warm-up pass (sim only)
 *   HQC_PLATFORM_HW   selects hardware perf_event_open path
 *
 * Requirements: 5.1, 5.2, 5.4
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "bench_common.h"
#include "api.h"
#include "symmetric.h"   /* prng_init */
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER N_ITER_DEFAULT
#endif

#ifndef N_WARMUP
#  define N_WARMUP N_WARMUP_DEFAULT
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

/* ══════════════════════════════════════════════════════════════════════════
 * Platform-specific sections
 * ══════════════════════════════════════════════════════════════════════════ */

#ifdef HQC_PLATFORM_HW
/* ── Hardware path: heap-allocated samples, perf_event_open timing ──────── */

#include <sched.h>

static uint64_t *samples_cyc;
static uint64_t *samples_ins;

/* Raw dump directory (NULL = no dump) */
static const char *raw_dump_dir = NULL;

static void dump_raw(const char *op, int n)
{
    if (!raw_dump_dir)
        return;
    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "%s/%s_%d_kem_%s_cycles.txt",
             raw_dump_dir, POLYMUL_NAME, SECURITY_LEVEL, op);
    FILE *f = fopen(path_buf, "w");
    if (!f) {
        fprintf(stderr, "WARNING: cannot open raw dump file: %s\n", path_buf);
        return;
    }
    for (int i = 0; i < n; i++) {
        fprintf(f, "%lu %lu\n",
                (unsigned long)samples_cyc[i],
                (unsigned long)samples_ins[i]);
    }
    fclose(f);
}

static void emit_csv_row_hw(const char *op, int n)
{
    perf_hw_sort_u64(samples_cyc, n);
    perf_hw_sort_u64(samples_ins, n);

    double mean_c = perf_hw_mean(samples_cyc, n);
    double std_c  = perf_hw_stddev(samples_cyc, n, mean_c);
    double mean_i = perf_hw_mean(samples_ins, n);
    double std_i  = perf_hw_stddev(samples_ins, n, mean_i);

    printf("%s,%d,"
           "%lu,%lu,%lu,%.1f,%.1f,"
           "%lu,%lu,%lu,%.1f,%.1f\n",
           op, n,
           (unsigned long)samples_cyc[0],
           (unsigned long)perf_hw_median(samples_cyc, n),
           (unsigned long)samples_cyc[n - 1],
           mean_c, std_c,
           (unsigned long)samples_ins[0],
           (unsigned long)perf_hw_median(samples_ins, n),
           (unsigned long)samples_ins[n - 1],
           mean_i, std_i);
}

int main(int argc, char **argv)
{
    /* Parse optional --raw-dir argument */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--raw-dir") == 0 && i + 1 < argc) {
            raw_dump_dir = argv[i + 1];
            i++;
        }
    }

    /* ── System checks ────────────────────────────────────────────────── */
    perf_hw_check_system();

    /* ── Allocate sample arrays ───────────────────────────────────────── */
    samples_cyc = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_ins = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_cyc || !samples_ins) {
        fprintf(stderr, "ERROR: failed to allocate sample arrays for %d iterations\n", N_ITER);
        return 1;
    }

    /* ── Initialize perf counters ─────────────────────────────────────── */
    perf_hw_ctx ctx;
    if (perf_hw_init(&ctx) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }

    /* ── Seed the SHAKE-256 PRNG ──────────────────────────────────────── */
    static const uint8_t dummy_seed[48] = { 0 };
    prng_init((uint8_t *)dummy_seed, NULL, 48, 0);

    /* ── Warm-up passes (prime i-cache, branch predictor, PRNG state) ── */
    for (int i = 0; i < N_WARMUP; i++) {
        crypto_kem_keypair(pk, sk);
        crypto_kem_enc(ct, ss_enc, pk);
        crypto_kem_dec(ss_dec, ct, sk);
    }

    /* ── Correctness check ────────────────────────────────────────────── */
    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, ss_enc, pk);
    crypto_kem_dec(ss_dec, ct, sk);
    if (memcmp(ss_enc, ss_dec, CRYPTO_BYTES) != 0) {
        fprintf(stderr, "ERROR: ss_enc != ss_dec — decaps failure!\n");
        perf_hw_close(&ctx);
        return 1;
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
    printf("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
           SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    printf("op,iterations,"
           "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
           "min_instret,median_instret,max_instret,mean_instret,stddev_instret\n");

    /* ── KeyGen ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        crypto_kem_keypair(pk, sk);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    dump_raw("keypair", N_ITER);
    emit_csv_row_hw("keypair", N_ITER);

    /* ── Encaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        crypto_kem_enc(ct, ss_enc, pk);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    dump_raw("encaps", N_ITER);
    emit_csv_row_hw("encaps", N_ITER);

    /* ── Decaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        crypto_kem_dec(ss_dec, ct, sk);
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
    dump_raw("decaps", N_ITER);
    emit_csv_row_hw("decaps", N_ITER);

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    perf_hw_close(&ctx);
    free(samples_cyc);
    free(samples_ins);

    return 0;
}

#else /* !HQC_PLATFORM_HW */
/* ── Simulation path: static arrays, rdcycle/rdinstret timing ──────────── */

/* Linker symbols bracketing .text.startup + .text (defined in sim/common/link.ld).
 * The difference gives the executable code size in bytes. */
extern char _text_start[];
extern char _text_end[];

/* ── Timing sample storage ───────────────────────────────────────────────── */
static uint64_t samples_cyc[N_ITER];
static uint64_t samples_ins[N_ITER];

/* ── Insertion sort (N_ITER is small for sim) ────────────────────────────── */
static void sort_u64(uint64_t *arr, int n)
{
    for (int i = 1; i < n; i++) {
        uint64_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static uint64_t median_u64(const uint64_t *sorted, int n)
{
    if (n % 2 == 1)
        return sorted[n / 2];
    return (sorted[n / 2 - 1] + sorted[n / 2]) / 2;
}

/* ── CSV emit helper (sim) ───────────────────────────────────────────────── */
static void emit_csv_row_sim(const char *op)
{
    sort_u64(samples_cyc, N_ITER);
    sort_u64(samples_ins, N_ITER);

    hqc_htif_puts(op);
    hqc_htif_puts(",");
    hqc_htif_putu64(N_ITER);
    /* cycles */
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(median_u64(samples_cyc, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[N_ITER - 1]);
    /* instret */
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(median_u64(samples_ins, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[N_ITER - 1]);
    hqc_htif_puts("\n");
}

/* ── main (simulation) ───────────────────────────────────────────────────── */
int main(void)
{
    /* Seed the vendor SHAKE-256 PRNG with a zero seed.
     * We only care about cycle counts; key values are irrelevant. */
    static const uint8_t dummy_seed[48] = { 0 };
    prng_init((uint8_t *)dummy_seed, NULL, 48, 0);

    /* ── Single warm-up pass (also serves as correctness check) ───────── */
#ifndef HQC_TRACE_MODE
    crypto_kem_keypair(pk, sk);
    crypto_kem_enc(ct, ss_enc, pk);
    crypto_kem_dec(ss_dec, ct, sk);
    if (memcmp(ss_enc, ss_dec, CRYPTO_BYTES) != 0) {
        hqc_htif_puts("ERROR: ss_enc != ss_dec — decaps failure!\n");
        hqc_htif_exit(1);
    }
#endif

    /* ── Emit self-describing metadata header ─────────────────────────── */
#ifndef HQC_SILENT
    uint64_t text_size = (uint64_t)(_text_end - _text_start);
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",polymul=" POLYMUL_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",codesize_text=");
    hqc_htif_putu64(text_size);
    hqc_htif_puts("\n");

    /* ── CSV header ───────────────────────────────────────────────── */
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles,"
                  "min_instret,median_instret,max_instret\n");
#endif

    /* ── KeyGen ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = BENCH_RDCYCLE();
        uint64_t t0_ins = hqc_rdinstret();
        crypto_kem_keypair(pk, sk);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row_sim("keypair");

    /* ── Encaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = BENCH_RDCYCLE();
        uint64_t t0_ins = hqc_rdinstret();
        crypto_kem_enc(ct, ss_enc, pk);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row_sim("encaps");

    /* ── Decaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = BENCH_RDCYCLE();
        uint64_t t0_ins = hqc_rdinstret();
        crypto_kem_dec(ss_dec, ct, sk);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row_sim("decaps");

    /* ── Anti-DCE: use results to prevent optimization ────────────────── */
    bench_consume(ss_dec, sizeof(ss_dec));

    hqc_htif_exit(0);
    return 0;
}

#endif /* HQC_PLATFORM_HW */
