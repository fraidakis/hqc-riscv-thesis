/* tests/bench/bench_keccak.c
 *
 * SHAKE-256 / SHA-3 (Keccak) micro-benchmark for HQC — unified for both
 * bare-metal RISC-V simulation (Spike ISS) and real hardware (BPI-F3).
 *
 * Replicates the FULL symmetric-primitive call pattern of a single HQC
 * decaps invocation, measuring the aggregate cost of all Keccak-f[1600]
 * permutations that occur during decapsulation.  This gives the true
 * "total Keccak cost in decaps" rather than the cost of a single
 * absorb+squeeze unit.
 *
 * ─── Decaps Keccak call pattern (HQC spec, 2024 final) ───────────────────
 *
 * The following symmetric operations execute during crypto_kem_dec():
 *
 *   1. hqc_dk_pke_from_string → xof_init (absorb 32+1 bytes, finalize)
 *                              → vect_sample_fixed_weight1 (squeeze ~3×ω bytes
 *                                with rejection, ω=66 for HQC-128)
 *      [This reconstructs the secret key vector y from dk_pke seed.]
 *
 *   2. hqc_pke_decrypt: no additional Keccak (only vect_mul + code_decode).
 *
 *   3. hash_h(hash_ek_kem, ek_pke)
 *      → SHA3-256 absorb PUBLIC_KEY_BYTES (~2241 bytes for HQC-128) + finalize
 *
 *   4. hash_g(K_theta_prime, hash_ek_kem, m_prime, salt)
 *      → SHA3-512 absorb (32 + 16 + 16 = 64 bytes) + finalize
 *
 *   5. Re-encryption: xof_init (absorb 32+1 bytes, finalize)
 *      → vect_sample_fixed_weight2 ×3 (squeeze 4×75 bytes each for HQC-128)
 *      → vect_set_random (squeeze VEC_N_SIZE_BYTES = 2209 bytes)
 *      [This regenerates r1, r2, e, h from theta_prime seed.]
 *
 *   6. hash_j(K_bar, hash_ek_kem, sigma, &c_kem_t)
 *      → SHA3-256 absorb (32 + 16 + 2209 + 2208 + 16 = ~4481 bytes) + finalize
 *
 * Total: ~6 XOF/hash initialisations, ~30-50 Keccak-f permutations depending
 * on security level and rejection sampling luck.
 *
 * ─── Platform selection ───────────────────────────────────────────────────
 *
 * Compile without HQC_PLATFORM_HW  → bare-metal sim (rdcycle via bench.h)
 * Compile with -DHQC_PLATFORM_HW=1 → Linux hw (perf_event_open via perf_hw.h)
 *
 * ─── Output CSV format ────────────────────────────────────────────────────
 *
 * Simulation:
 *   # level=<L>,keccak=<KECCAK_NAME>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   keccak,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *
 * Hardware:
 *   # level=<L>,keccak=<KECCAK_NAME>,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,
 *   min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   keccak,...
 *
 * Compile-time knobs:
 *   SECURITY_LEVEL  128 | 192 | 256
 *   KECCAK_NAME     string literal e.g. "ref" or "zvbb_x2_soa"
 *   N_ITER          (optional, default from bench_common.h)
 *   N_WARMUP        (optional, default from bench_common.h)
 *   HQC_SILENT      suppresses HTIF metadata headers (sim only)
 *   HQC_TRACE_MODE  trace/hist-only mode that compiles out warm-up calls (sim only)
 *
 * Requirements: 5.1, 5.2, 5.4
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "symmetric.h"
#include "vector.h"
#include "bench_common.h"
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER N_ITER_DEFAULT
#endif

#ifndef N_WARMUP
#  define N_WARMUP N_WARMUP_DEFAULT
#endif

#ifndef KECCAK_NAME
#  define KECCAK_NAME "ref"
#endif

/* ── Platform-specific declarations ──────────────────────────────────────── */

#ifdef HQC_PLATFORM_HW

/* Timing sample storage (heap-allocated for large N_ITER on hw) */
static uint64_t *samples_cyc;
static uint64_t *samples_ins;

/* Raw dump file path (NULL = no dump) */
static const char *raw_dump_path = NULL;

#else /* Simulation */

/* Linker symbols bracketing .text */
extern char _text_start[];
extern char _text_end[];

/* Timing sample storage (statically allocated for sim) */
static uint64_t samples_cyc[N_ITER];
static uint64_t samples_ins[N_ITER];

#endif /* HQC_PLATFORM_HW */

/* ── Static buffers (sized to match real decaps operands) ────────────────── */

/* Seeds and keys */
static uint8_t seed_dk[SEED_BYTES];
static uint8_t seed_theta[SEED_BYTES];
static uint8_t ek_pke[PUBLIC_KEY_BYTES];
static uint8_t hash_ek[SEED_BYTES];
static uint8_t m_prime[PARAM_SECURITY_BYTES];
static uint8_t salt[SALT_BYTES];
static uint8_t sigma[PARAM_SECURITY_BYTES];
static uint8_t K_theta[SHARED_SECRET_BYTES + SEED_BYTES];
static uint8_t K_bar[SHARED_SECRET_BYTES];

/* Ciphertext structure for hash_j */
static ciphertext_kem_t c_kem_buf;

/* Vector buffers for sampling (written but not used for polymul) */
static uint64_t vec_y[VEC_N_SIZE_64];
static uint64_t vec_r1[VEC_N_SIZE_64];
static uint64_t vec_r2[VEC_N_SIZE_64];
static uint64_t vec_e[VEC_N_SIZE_64];
static uint64_t vec_h[VEC_N_SIZE_64];

/* ── Sorting / statistics helpers ────────────────────────────────────────── */

#ifdef HQC_PLATFORM_HW

/* CSV emit helper (hw: includes mean/stddev columns) */
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

/* Raw per-iteration dump */
static void dump_raw(int n)
{
    if (!raw_dump_path)
        return;
    FILE *f = fopen(raw_dump_path, "w");
    if (!f) {
        fprintf(stderr, "WARNING: cannot open raw dump file: %s\n", raw_dump_path);
        return;
    }
    for (int i = 0; i < n; i++) {
        fprintf(f, "%lu %lu\n",
                (unsigned long)samples_cyc[i],
                (unsigned long)samples_ins[i]);
    }
    fclose(f);
}

#else /* Simulation */

static void sort_u64(uint64_t *arr, int n)
{
    for (int i = 1; i < n; i++) {
        uint64_t key = arr[i];
        int j = i - 1;
        while (j >= 0 && (int64_t)(arr[j] - key) > 0) {
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

#endif /* HQC_PLATFORM_HW */

/* ── Replicate decaps Keccak call pattern ────────────────────────────────── */
static void keccak_decaps_pattern(void)
{
    shake256_xof_ctx xof_ctx;

    /* Step 1: Reconstruct secret key y from dk_pke seed */
    xof_init(&xof_ctx, seed_dk, SEED_BYTES);
    vect_sample_fixed_weight1(&xof_ctx, vec_y, PARAM_OMEGA);

    /* Step 2: (vect_mul + code_decode — NOT measured here) */

    /* Step 3: hash_h — SHA3-256 over the full public key */
    hash_h(hash_ek, ek_pke);

    /* Step 4: hash_g — SHA3-512 over (hash_ek || m' || salt) */
    hash_g(K_theta, hash_ek, m_prime, salt);

    /* Step 5: Re-encryption sampling from theta_prime */
    xof_init(&xof_ctx, seed_theta, SEED_BYTES);
    vect_sample_fixed_weight2(&xof_ctx, vec_r2, PARAM_OMEGA_R);
    vect_sample_fixed_weight2(&xof_ctx, vec_e, PARAM_OMEGA_E);
    vect_sample_fixed_weight2(&xof_ctx, vec_r1, PARAM_OMEGA_R);
    vect_set_random(&xof_ctx, vec_h);

    /* Step 6: hash_j — SHA3-256 over (hash_ek || sigma || u || v || salt) */
    hash_j(K_bar, hash_ek, sigma, &c_kem_buf);
}

/* ── main ────────────────────────────────────────────────────────────────── */
#ifdef HQC_PLATFORM_HW
int main(int argc, char **argv)
#else
int main(void)
#endif
{
#ifdef HQC_PLATFORM_HW
    /* Parse optional --raw-dir argument */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--raw-dir") == 0 && i + 1 < argc) {
            static char path_buf[512];
            snprintf(path_buf, sizeof(path_buf), "%s/keccak_%d_cycles.txt",
                     argv[i + 1], SECURITY_LEVEL);
            raw_dump_path = path_buf;
            i++;
        }
    }

    /* System checks */
    perf_hw_check_system();

    /* Allocate sample arrays */
    samples_cyc = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_ins = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_cyc || !samples_ins) {
        fprintf(stderr, "ERROR: failed to allocate sample arrays for %d iterations\n", N_ITER);
        return 1;
    }
#endif /* HQC_PLATFORM_HW */

    /* ── Initialize input buffers with non-trivial patterns ───────────── */
    for (size_t i = 0; i < SEED_BYTES; i++) {
        seed_dk[i] = (uint8_t)(0xA5U ^ (uint8_t)(i * 0x9eU));
        seed_theta[i] = (uint8_t)(0x3CU ^ (uint8_t)(i * 0x7bU));
    }
    for (size_t i = 0; i < PUBLIC_KEY_BYTES; i++)
        ek_pke[i] = (uint8_t)(0x55U ^ (uint8_t)(i * 0xc3U));
    for (size_t i = 0; i < PARAM_SECURITY_BYTES; i++) {
        m_prime[i] = (uint8_t)(0x1FU ^ (uint8_t)(i * 0xabU));
        sigma[i] = (uint8_t)(0x72U ^ (uint8_t)(i * 0xd1U));
    }
    for (size_t i = 0; i < SALT_BYTES; i++)
        salt[i] = (uint8_t)(0xE7U ^ (uint8_t)(i * 0x4fU));
    memset(&c_kem_buf, 0xAB, sizeof(c_kem_buf));

#ifdef HQC_PLATFORM_HW
    /* ── Initialize perf counters ─────────────────────────────────────── */
    perf_hw_ctx ctx;
    if (perf_hw_init(&ctx) != 0) {
        fprintf(stderr, "ERROR: cannot initialize perf counters\n");
        return 1;
    }

    /* ── Warm-up passes (prime i-cache, branch predictor) ─────────────── */
    for (int i = 0; i < N_WARMUP; i++) {
        keccak_decaps_pattern();
    }

    /* ── Emit metadata header ─────────────────────────────────────────── */
    printf("# level=%d,keccak=%s,n_iter=%d,n_warmup=%d,platform=hw\n",
           SECURITY_LEVEL, KECCAK_NAME, N_ITER, N_WARMUP);
    printf("op,iterations,"
           "min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,"
           "min_instret,median_instret,max_instret,mean_instret,stddev_instret\n");

    /* ── Timed passes ─────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        keccak_decaps_pattern();
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }

    /* Raw dump (unsorted, original order — must happen before sort) */
    dump_raw(N_ITER);

    emit_csv_row_hw("keccak", N_ITER);

    /* ── Anti-DCE ─────────────────────────────────────────────────────── */
    if (K_bar[0] == 0xde && vec_h[0] == 0xdeadbeefdeadbeefULL)
        printf("# (dead code marker)\n");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    perf_hw_close(&ctx);
    free(samples_cyc);
    free(samples_ins);

#else /* Simulation */

    /* ── Emit metadata header ────────────────────────────────────────────── */
#ifndef HQC_SILENT
    uint64_t text_size = (uint64_t)(_text_end - _text_start);
    hqc_htif_puts("# level=");
    hqc_htif_putu64(SECURITY_LEVEL);
    hqc_htif_puts(",keccak=" KECCAK_NAME ",n_iter=");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",codesize_text=");
    hqc_htif_putu64(text_size);
    hqc_htif_puts("\n");
#endif

    /* ── Warm-up call ────────────────────────────────────────────────────── */
#ifndef HQC_TRACE_MODE
    keccak_decaps_pattern();
#endif

    /* ── Timed passes ────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        keccak_decaps_pattern();
        uint64_t t1_cyc = hqc_rdcycle();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }

    sort_u64(samples_cyc, N_ITER);
    sort_u64(samples_ins, N_ITER);

    /* ── Emit CSV ────────────────────────────────────────────────────────── */
#ifndef HQC_SILENT
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles,"
                  "min_instret,median_instret,max_instret\n");
#endif
    hqc_htif_puts("keccak,");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(median_u64(samples_cyc, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_cyc[N_ITER - 1]);
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[0]);
    hqc_htif_puts(",");
    hqc_htif_putu64(median_u64(samples_ins, N_ITER));
    hqc_htif_puts(",");
    hqc_htif_putu64(samples_ins[N_ITER - 1]);
    hqc_htif_puts("\n");

    /* Use output buffers to defeat dead-code elimination. */
    if (K_bar[0] == 0xde && vec_h[0] == 0xdeadbeefdeadbeefULL)
        hqc_htif_puts("# (dead code marker)\n");

    hqc_htif_exit(0);
#endif /* HQC_PLATFORM_HW */

    return 0;
}
