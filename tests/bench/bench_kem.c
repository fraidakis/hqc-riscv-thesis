/* tests/bench/bench_kem.c
 *
 * Cycle-count benchmark harness for HQC KEM on bare-metal RISC-V.
 *
 * Runs N_ITER iterations of each of the three KEM operations (KeyGen,
 * Encaps, Decaps), measures rdcycle and minstret before and after each,
 * and emits a CSV summary to the HTIF console.
 *
 * Output format (one header comment + one CSV header + one data row per op):
 *   # level=<L>,polymul=<P>,n_iter=<N>,codesize_text=<bytes>
 *   op,iterations,min_cycles,median_cycles,max_cycles,min_instret,median_instret,max_instret
 *   keypair,<N>,<min_c>,<med_c>,<max_c>,<min_i>,<med_i>,<max_i>
 *   encaps,<N>,...
 *   decaps,<N>,...
 *
 * Parsed by scripts/report/extract_results.py to produce thesis tables.
 *
 * Compile-time knobs (set by CMakeLists.txt):
 *   SECURITY_LEVEL    128 | 192 | 256
 *   POLYMUL_NAME      string literal, e.g. "karatsuba_zbc"
 *   HQC_BENCH_MODE=1  ensures AES-free randombytes()
 *   N_ITER            (optional, default 1)
 *   HQC_SILENT        suppresses HTIF metadata headers in normal Spike bench runs
 *   HQC_TRACE_MODE    trace/hist-only mode that compiles out warm-up correctness pass
 *
 * Warmup: in normal bench/cache builds, a single non-timed pass of all
 * three operations is run before timed N_ITER passes. On Spike it validates
 * correctness and primes PRNG state; on real hardware it also primes i-cache.
 * In HQC_TRACE_MODE this pass is compiled out so trace/histogram capture
 * exactly one functional KEM cycle without warm-up inflation.
 *
 * Note: only use on Spike for cycle-accurate measurements.  QEMU cycle
 * counts are unreliable (QEMU does not model the pipeline).
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "api.h"
#include "symmetric.h"  /* prng_init */
#include "bench.h"
#include "parameters.h"

#ifndef N_ITER
#  define N_ITER 1
#endif

#ifndef POLYMUL_NAME
#  define POLYMUL_NAME "unknown"
#endif

/* Linker symbols bracketing .text.startup + .text (defined in sim/common/link.ld).
 * The difference gives the executable code size in bytes. */
extern char _text_start[];
extern char _text_end[];

/* ── Static buffers ──────────────────────────────────────────────────────── */
static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

/* ── Timing sample storage ───────────────────────────────────────────────── */
static uint64_t samples_cyc[N_ITER];
static uint64_t samples_ins[N_ITER];

/* Insertion sort (N_ITER <= 100, not performance-critical) */
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

/* ── CSV emit helper ─────────────────────────────────────────────────────── */
static void emit_csv_row(const char *op)
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

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    /* Seed the vendor SHAKE-256 PRNG with a zero seed.
     * We only care about cycle counts; key values are irrelevant. */
    static const uint8_t dummy_seed[48] = { 0 };
    prng_init((uint8_t *)dummy_seed, NULL, 48, 0);

    /* ── Single warm-up pass (also serves as correctness check) ───────── */
    /* One call primes the PRNG state and (on real hardware) the i-cache.
     * On Spike there are no caches, so this is purely the correctness gate. */
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
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        crypto_kem_keypair(pk, sk);
        uint64_t t1_cyc = hqc_rdcycle();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row("keypair");

    /* ── Encaps ───────────────────────────────────────────────────────── */
    /* Use pk from last keypair iteration */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        crypto_kem_enc(ct, ss_enc, pk);
        uint64_t t1_cyc = hqc_rdcycle();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row("encaps");

    /* ── Decaps ───────────────────────────────────────────────────────── */
    /* Use ct from last encaps iteration */
    for (int i = 0; i < N_ITER; i++) {
        uint64_t t0_cyc = hqc_rdcycle();
        uint64_t t0_ins = hqc_rdinstret();
        crypto_kem_dec(ss_dec, ct, sk);
        uint64_t t1_cyc = hqc_rdcycle();
        uint64_t t1_ins = hqc_rdinstret();
        samples_cyc[i] = t1_cyc - t0_cyc;
        samples_ins[i] = t1_ins - t0_ins;
    }
    emit_csv_row("decaps");

    hqc_htif_exit(0);
    return 0;
}
