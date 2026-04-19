/* tests/bench/bench_kem.c
 *
 * Cycle-count benchmark harness for HQC KEM on bare-metal RISC-V.
 *
 * Runs N_ITER iterations of each of the three KEM operations (KeyGen,
 * Encaps, Decaps), measures rdcycle before and after each, and emits
 * a CSV summary to the HTIF console.
 *
 * Output format (one header line + one data line per operation):
 *   op,iterations,min_cycles,median_cycles,max_cycles
 *   keypair,<N>,<min>,<median>,<max>
 *   encaps,<N>,<min>,<median>,<max>
 *   decaps,<N>,<min>,<median>,<max>
 *
 * This is parsed by scripts/bench/collect_results.py (to be written)
 * to produce the thesis performance tables.
 *
 * Compile-time knobs (set by CMakeLists.txt):
 *   SECURITY_LEVEL    128 | 192 | 256
 *   HQC_BENCH_MODE=1  ensures AES-free randombytes()
 *   N_ITER            (optional, default 10)
 *
 * Note: only use on Spike for cycle-accurate measurements.  QEMU cycle
 * counts are unreliable (QEMU does not model the pipeline; rdcycle is a
 * software counter that does not reflect real latency).
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "api.h"
#include "randombytes.h"
#include "bench.h"
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER 10
#endif

/* ── Static buffers ──────────────────────────────────────────────────────── */
static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

/* ── Timing sample storage ───────────────────────────────────────────────── */
static uint64_t samples[N_ITER];

/* Simple insertion-sort (N_ITER <= 100, not performance-critical) */
static void sort_samples(void)
{
    for (int i = 1; i < N_ITER; i++) {
        uint64_t key = samples[i];
        int j = i - 1;
        while (j >= 0 && samples[j] > key) {
            samples[j + 1] = samples[j];
            j--;
        }
        samples[j + 1] = key;
    }
}

static uint64_t min_sample(void)  { return samples[0]; }
static uint64_t max_sample(void)  { return samples[N_ITER - 1]; }
static uint64_t median_sample(void)
{
    if (N_ITER % 2 == 1)
        return samples[N_ITER / 2];
    return (samples[N_ITER / 2 - 1] + samples[N_ITER / 2]) / 2;
}

/* ── CSV emit helpers ────────────────────────────────────────────────────── */
static void emit_csv_row(const char *op)
{
    sort_samples();
    hqc_htif_puts(op);
    hqc_htif_puts(",");
    hqc_htif_putu64(N_ITER);
    hqc_htif_puts(",");
    hqc_htif_putu64(min_sample());
    hqc_htif_puts(",");
    hqc_htif_putu64(median_sample());
    hqc_htif_puts(",");
    hqc_htif_putu64(max_sample());
    hqc_htif_puts("\n");
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Seed the (bench-mode) PRNG — no-op but keeps source compatibility */
    static const uint8_t dummy_seed[48] = { 0 };
    randombytes_init(dummy_seed);

    /* ── KeyGen ────────────────────────────────────────────────────────── */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        crypto_kem_keypair(pk, sk);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }

    /* ── Encaps ────────────────────────────────────────────────────────── */
    /* Use key from last keypair iteration (any valid key will do) */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        crypto_kem_enc(ct, ss_enc, pk);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }

    /* ── Decaps ────────────────────────────────────────────────────────── */
    /* Use ct from last enc iteration */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        crypto_kem_dec(ss_dec, ct, sk);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }

    /* ── Verify correctness (sanity check, not a KAT) ─────────────────── */
    if (memcmp(ss_enc, ss_dec, CRYPTO_BYTES) != 0) {
        hqc_htif_puts("ERROR: ss_enc != ss_dec — decaps failure!\n");
        hqc_htif_exit(1);
    }

    /* ── Emit CSV ──────────────────────────────────────────────────────── */
    hqc_htif_puts("op,iterations,min_cycles,median_cycles,max_cycles\n");

    /* Re-run timed passes now that warm-up is done — re-use sample arrays */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        crypto_kem_keypair(pk, sk);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }
    emit_csv_row("keypair");

    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        crypto_kem_enc(ct, ss_enc, pk);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }
    emit_csv_row("encaps");

    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0 = hqc_rdcycle();
        crypto_kem_dec(ss_dec, ct, sk);
        uint64_t t1 = hqc_rdcycle();
        samples[i] = t1 - t0;
    }
    emit_csv_row("decaps");

    hqc_htif_exit(0);
    return 0;
}
