/* tests/bench/bench_hw_keccak.c
 *
 * SHAKE-256 / SHA-3 (Keccak) micro-benchmark for HQC on real RISC-V hardware.
 *
 * Mirrors bench_keccak.c but uses perf_event_open instead of bare-metal rdcycle.
 * Replicates the FULL symmetric-primitive call pattern of a single HQC decaps
 * invocation, measuring the aggregate cost of all Keccak-f[1600] permutations.
 *
 * Output format (compatible with scripts/report/extract_results.py):
 *   # level=<L>,keccak=ref,n_iter=<N>,n_warmup=<W>,platform=hw
 *   op,iterations,min_cycles,median_cycles,max_cycles,mean_cycles,stddev_cycles,
 *   min_instret,median_instret,max_instret,mean_instret,stddev_instret
 *   keccak,...
 *
 * Compile (native on board via Makefile.hw):
 *   make -f Makefile.hw LEVEL=128 POLYMUL=ref_scalar MODE=publish keccak
 *
 * Run:
 *   sudo nice -n -20 taskset -c 7 ./bench_hw_keccak_hqc128
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "symmetric.h"
#include "vector.h"
#include "parameters.h"
#include "perf_hw.h"

#ifndef N_ITER
#  define N_ITER 10000
#endif

#ifndef N_WARMUP
#  define N_WARMUP 100
#endif

#ifndef KECCAK_NAME
#  define KECCAK_NAME "ref"
#endif

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

/* ── Timing sample storage (heap-allocated) ──────────────────────────────── */
static uint64_t *samples_cyc;
static uint64_t *samples_ins;

/* ── Raw dump file path (NULL = no dump) ─────────────────────────────────── */
static const char *raw_dump_path = NULL;

/* ── CSV emit helper ─────────────────────────────────────────────────────── */
static void emit_csv_row(const char *op, int n)
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

/* ── Raw per-iteration dump ──────────────────────────────────────────────── */
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
int main(int argc, char **argv)
{
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

    /* ── System checks ────────────────────────────────────────────────── */
    perf_hw_check_system();

    /* ── Allocate sample arrays ───────────────────────────────────────── */
    samples_cyc = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    samples_ins = (uint64_t *)malloc((size_t)N_ITER * sizeof(uint64_t));
    if (!samples_cyc || !samples_ins) {
        fprintf(stderr, "ERROR: failed to allocate sample arrays for %d iterations\n", N_ITER);
        return 1;
    }

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

    /* ── Timed passes ────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        perf_hw_sample s;
        perf_hw_start(&ctx);
        keccak_decaps_pattern();
        perf_hw_stop(&ctx);
        perf_hw_read(&ctx, &s);
        samples_cyc[i] = s.cycles;
        samples_ins[i] = s.instret;
    }

    /* ── Raw dump (unsorted, original order — must happen before sort) ── */
    dump_raw(N_ITER);

    emit_csv_row("keccak", N_ITER);

    /* ── Anti-DCE ─────────────────────────────────────────────────────── */
    if (K_bar[0] == 0xde && vec_h[0] == 0xdeadbeefdeadbeefULL)
        printf("# (dead code marker)\n");

    /* ── Cleanup ──────────────────────────────────────────────────────── */
    perf_hw_close(&ctx);
    free(samples_cyc);
    free(samples_ins);

    return 0;
}
