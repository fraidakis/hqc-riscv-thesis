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

#ifdef PERF_HW_EXTENDED
/* Extended PMU sample arrays (one per counter per iteration) */
static uint64_t *samples_l1d_miss;
static uint64_t *samples_l1d_access;
static uint64_t *samples_dtlb_miss;
static uint64_t *samples_br_mispred;
static uint64_t *samples_stall_fe;
static uint64_t *samples_stall_be;
static uint64_t *samples_vec_inst;
static uint64_t *samples_vpu_stall;
/* Pass 3: vector memory counters */
static uint64_t *samples_vec_load;
static uint64_t *samples_vec_store;
#endif

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
    /* cycles: sort then compute quartiles */
    perf_hw_sort_u64(samples_cyc, n);
    uint64_t cyc_min    = samples_cyc[0];
    uint64_t cyc_p25    = perf_hw_p25(samples_cyc, n);
    uint64_t cyc_median = perf_hw_median(samples_cyc, n);
    uint64_t cyc_p75    = perf_hw_p75(samples_cyc, n);
    uint64_t cyc_iqr    = cyc_p75 - cyc_p25;
    uint64_t cyc_max    = samples_cyc[n - 1];

    /* instret: deterministic for fixed-data kernels → single median value */
    uint64_t ins_median = perf_hw_median(samples_ins, n);

    printf("%s,%d,"
           "%lu,%lu,%lu,%lu,%lu,%lu,"
           "%lu",
           op, n,
           (unsigned long)cyc_min,
           (unsigned long)cyc_p25,
           (unsigned long)cyc_median,
           (unsigned long)cyc_p75,
           (unsigned long)cyc_iqr,
           (unsigned long)cyc_max,
           (unsigned long)ins_median);

#ifdef PERF_HW_EXTENDED
    /* Sort extended arrays and emit medians + derived miss rate */
    perf_hw_sort_u64(samples_l1d_miss, n);
    perf_hw_sort_u64(samples_l1d_access, n);
    perf_hw_sort_u64(samples_dtlb_miss, n);
    perf_hw_sort_u64(samples_br_mispred, n);
    perf_hw_sort_u64(samples_stall_fe, n);
    perf_hw_sort_u64(samples_stall_be, n);
    perf_hw_sort_u64(samples_vec_inst, n);
    perf_hw_sort_u64(samples_vpu_stall, n);

    uint64_t med_l1d_miss  = perf_hw_median(samples_l1d_miss, n);
    uint64_t med_l1d_acc   = perf_hw_median(samples_l1d_access, n);
    uint64_t med_dtlb      = perf_hw_median(samples_dtlb_miss, n);
    uint64_t med_br        = perf_hw_median(samples_br_mispred, n);
    uint64_t med_fe        = perf_hw_median(samples_stall_fe, n);
    uint64_t med_be        = perf_hw_median(samples_stall_be, n);
    uint64_t med_vec       = perf_hw_median(samples_vec_inst, n);
    uint64_t med_vpu       = perf_hw_median(samples_vpu_stall, n);

    double miss_rate = (med_l1d_acc > 0)
        ? 100.0 * (double)med_l1d_miss / (double)med_l1d_acc
        : 0.0;

    printf(",%lu,%lu,%.2f%%,%lu,%lu,%lu,%lu,%lu,%lu",
           (unsigned long)med_l1d_miss,
           (unsigned long)med_l1d_acc,
           miss_rate,
           (unsigned long)med_dtlb,
           (unsigned long)med_br,
           (unsigned long)med_fe,
           (unsigned long)med_be,
           (unsigned long)med_vec,
           (unsigned long)med_vpu);

    /* Pass 3 vector memory counters */
    perf_hw_sort_u64(samples_vec_load, n);
    perf_hw_sort_u64(samples_vec_store, n);
    uint64_t med_vec_load  = perf_hw_median(samples_vec_load, n);
    uint64_t med_vec_store = perf_hw_median(samples_vec_store, n);
    printf(",%lu,%lu",
           (unsigned long)med_vec_load,
           (unsigned long)med_vec_store);
#endif /* PERF_HW_EXTENDED */

    printf("\n");
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

#ifdef PERF_HW_EXTENDED
    samples_l1d_miss  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_l1d_access= (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_dtlb_miss = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_br_mispred= (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_stall_fe  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_stall_be  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_vec_inst  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_vpu_stall = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_vec_load  = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_vec_store = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_l1d_miss || !samples_l1d_access || !samples_dtlb_miss ||
        !samples_br_mispred || !samples_stall_fe || !samples_stall_be ||
        !samples_vec_inst || !samples_vpu_stall ||
        !samples_vec_load || !samples_vec_store) {
        fprintf(stderr, "ERROR: failed to allocate extended sample arrays\n");
        return 1;
    }
#endif

    /* ── Initialize perf counters ─────────────────────────────────────── */
#ifdef PERF_HW_EXTENDED
    /* Two-pass approach: basic counters for clean cycle/instret measurement,
     * then extended counters for microarchitectural profiling.
     * This avoids PMU multiplexing overhead contaminating cycle counts. */
    perf_hw_ctx ctx_basic;
    if (perf_hw_init(&ctx_basic) != 0) {
        fprintf(stderr, "ERROR: cannot initialize basic perf counters\n");
        return 1;
    }
    perf_hw_ext_ctx ctx_ext;
    if (perf_hw_ext_init(&ctx_ext) != 0) {
        fprintf(stderr, "ERROR: cannot initialize extended perf counters\n");
        return 1;
    }
    perf_hw_vecmem_ctx ctx_vecmem;
    perf_hw_vecmem_init(&ctx_vecmem);
#else
    perf_hw_ctx ctx_basic;
    if (perf_hw_init(&ctx_basic) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }
#endif

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
#ifdef PERF_HW_EXTENDED
        perf_hw_ext_close(&ctx_ext);
#endif
        perf_hw_close(&ctx_basic);
        return 1;
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
#ifdef PERF_HW_EXTENDED
    printf("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw,extended=1\n",
           SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    perf_hw_ext_print_csv_header();
#else
    printf("# level=%d,polymul=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
           SECURITY_LEVEL, POLYMUL_NAME, N_ITER, N_WARMUP);
    perf_hw_basic_print_csv_header();
#endif

    /* ── KeyGen ───────────────────────────────────────────────────────── */
    /* Pass 1: clean cycles + instret */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx_basic);
        crypto_kem_keypair(pk, sk);
        perf_hw_stop(&ctx_basic);
        perf_hw_read(&ctx_basic, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
#ifdef PERF_HW_EXTENDED
    /* Pass 2: extended PMU counters */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_ext_sample s;
        perf_hw_ext_start(&ctx_ext);
        crypto_kem_keypair(pk, sk);
        perf_hw_ext_stop(&ctx_ext);
        perf_hw_ext_read(&ctx_ext, &s);
        samples_l1d_miss[i]   = s.l1d_load_miss;
        samples_l1d_access[i] = s.l1d_access;
        samples_dtlb_miss[i]  = s.dtlb_load_miss;
        samples_br_mispred[i] = s.br_mispred;
        samples_stall_fe[i]   = s.stall_frontend;
        samples_stall_be[i]   = s.stall_backend;
        samples_vec_inst[i]   = s.vector_inst;
        samples_vpu_stall[i]  = s.vpu_stall;
    }
    /* Pass 3: vector memory counters (dedicated, avoids multiplexing) */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_vecmem_sample vs;
        perf_hw_vecmem_start(&ctx_vecmem);
        crypto_kem_keypair(pk, sk);
        perf_hw_vecmem_stop(&ctx_vecmem);
        perf_hw_vecmem_read(&ctx_vecmem, &vs);
        samples_vec_load[i]  = vs.vec_load;
        samples_vec_store[i] = vs.vec_store;
    }
#endif
    dump_raw("keypair", N_ITER);
    emit_csv_row_hw("keypair", N_ITER);

    /* ── Encaps ───────────────────────────────────────────────────────── */
    /* Pass 1: clean cycles + instret */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx_basic);
        crypto_kem_enc(ct, ss_enc, pk);
        perf_hw_stop(&ctx_basic);
        perf_hw_read(&ctx_basic, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
#ifdef PERF_HW_EXTENDED
    /* Pass 2: extended PMU counters */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_ext_sample s;
        perf_hw_ext_start(&ctx_ext);
        crypto_kem_enc(ct, ss_enc, pk);
        perf_hw_ext_stop(&ctx_ext);
        perf_hw_ext_read(&ctx_ext, &s);
        samples_l1d_miss[i]   = s.l1d_load_miss;
        samples_l1d_access[i] = s.l1d_access;
        samples_dtlb_miss[i]  = s.dtlb_load_miss;
        samples_br_mispred[i] = s.br_mispred;
        samples_stall_fe[i]   = s.stall_frontend;
        samples_stall_be[i]   = s.stall_backend;
        samples_vec_inst[i]   = s.vector_inst;
        samples_vpu_stall[i]  = s.vpu_stall;
    }
    /* Pass 3: vector memory counters (dedicated, avoids multiplexing) */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_vecmem_sample vs;
        perf_hw_vecmem_start(&ctx_vecmem);
        crypto_kem_enc(ct, ss_enc, pk);
        perf_hw_vecmem_stop(&ctx_vecmem);
        perf_hw_vecmem_read(&ctx_vecmem, &vs);
        samples_vec_load[i]  = vs.vec_load;
        samples_vec_store[i] = vs.vec_store;
    }
#endif
    dump_raw("encaps", N_ITER);
    emit_csv_row_hw("encaps", N_ITER);

    /* ── Decaps ───────────────────────────────────────────────────────── */
    /* Pass 1: clean cycles + instret */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx_basic);
        crypto_kem_dec(ss_dec, ct, sk);
        perf_hw_stop(&ctx_basic);
        perf_hw_read(&ctx_basic, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }
#ifdef PERF_HW_EXTENDED
    /* Pass 2: extended PMU counters */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_ext_sample s;
        perf_hw_ext_start(&ctx_ext);
        crypto_kem_dec(ss_dec, ct, sk);
        perf_hw_ext_stop(&ctx_ext);
        perf_hw_ext_read(&ctx_ext, &s);
        samples_l1d_miss[i]   = s.l1d_load_miss;
        samples_l1d_access[i] = s.l1d_access;
        samples_dtlb_miss[i]  = s.dtlb_load_miss;
        samples_br_mispred[i] = s.br_mispred;
        samples_stall_fe[i]   = s.stall_frontend;
        samples_stall_be[i]   = s.stall_backend;
        samples_vec_inst[i]   = s.vector_inst;
        samples_vpu_stall[i]  = s.vpu_stall;
    }
    /* Pass 3: vector memory counters (dedicated, avoids multiplexing) */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_vecmem_sample vs;
        perf_hw_vecmem_start(&ctx_vecmem);
        crypto_kem_dec(ss_dec, ct, sk);
        perf_hw_vecmem_stop(&ctx_vecmem);
        perf_hw_vecmem_read(&ctx_vecmem, &vs);
        samples_vec_load[i]  = vs.vec_load;
        samples_vec_store[i] = vs.vec_store;
    }
#endif
    dump_raw("decaps", N_ITER);
    emit_csv_row_hw("decaps", N_ITER);

    /* ── Cleanup ──────────────────────────────────────────────────────── */
#ifdef PERF_HW_EXTENDED
    perf_hw_ext_close(&ctx_ext);
    perf_hw_vecmem_close(&ctx_vecmem);
    free(samples_l1d_miss);
    free(samples_l1d_access);
    free(samples_dtlb_miss);
    free(samples_br_mispred);
    free(samples_stall_fe);
    free(samples_stall_be);
    free(samples_vec_inst);
    free(samples_vpu_stall);
    free(samples_vec_load);
    free(samples_vec_store);
#endif
    perf_hw_close(&ctx_basic);
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
        uint64_t t0_ins = hqc_read_minstret();
        crypto_kem_keypair(pk, sk);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_read_minstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row_sim("keypair");

    /* ── Encaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = BENCH_RDCYCLE();
        uint64_t t0_ins = hqc_read_minstret();
        crypto_kem_enc(ct, ss_enc, pk);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_read_minstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row_sim("encaps");

    /* ── Decaps ───────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = BENCH_RDCYCLE();
        uint64_t t0_ins = hqc_read_minstret();
        crypto_kem_dec(ss_dec, ct, sk);
        uint64_t t1_cyc = BENCH_RDCYCLE();
        uint64_t t1_ins = hqc_read_minstret();
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
