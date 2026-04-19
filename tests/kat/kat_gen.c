/* tests/kat/kat_gen.c
 *
 * Bare-metal KAT runner for HQC on RISC-V (Spike / QEMU).
 *
 * Behaviour:
 *   For each of 100 KAT test vectors:
 *     1. Init the AES-CTR DRBG with the pre-generated seed from kat_seeds_<LEVEL>.h
 *        (produced by gen_kat_seeds.py from the official .rsp file).
 *     2. Call crypto_kem_keypair, crypto_kem_enc, crypto_kem_dec.
 *     3. Verify ss_enc == ss_dec (shared-secret match).
 *     4. Emit the shared secret as 64 hex bytes on a single HTIF line:
 *          "ss: <hex>\n"
 *        This line format is parsed by kat_check.py.
 *   After all 100 vectors: emit "PASS <n>/100\n" or "FAIL at vector <k>\n"
 *   then call hqc_htif_exit(0) or hqc_htif_exit(1).
 *
 * Compile-time requirements:
 *   SECURITY_LEVEL must be defined (128 | 192 | 256) by CMakeLists.txt.
 *   HQC_KAT_MODE=1 must be set (ensures AES-CTR DRBG is used).
 *
 * The generated KAT seed header is expected at:
 *   tests/kat/generated/kat_seeds_<SECURITY_LEVEL>.h
 * and provides:
 *   static const uint8_t kat_seeds[100][48];  — one 48-byte entropy seed per vector
 *   static const uint8_t kat_ss[100][64];     — expected shared secrets (from .rsp)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "api.h"
#include "randombytes.h"
#include "bench.h"   /* hqc_htif_puts, hqc_htif_putu64, hqc_htif_exit */
#include "parameters.h"

/* Include the pre-generated seed+expected-output table. */
#if SECURITY_LEVEL == 128
#  include "generated/kat_seeds_128.h"
#elif SECURITY_LEVEL == 192
#  include "generated/kat_seeds_192.h"
#elif SECURITY_LEVEL == 256
#  include "generated/kat_seeds_256.h"
#else
#  error "SECURITY_LEVEL must be 128, 192, or 256"
#endif

#define KAT_COUNT 100

/* ── Static key/ciphertext/ss buffers (avoid large stack frames) ─────────── */
static uint8_t pk[CRYPTO_PUBLICKEYBYTES];
static uint8_t sk[CRYPTO_SECRETKEYBYTES];
static uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
static uint8_t ss_enc[CRYPTO_BYTES];
static uint8_t ss_dec[CRYPTO_BYTES];

/* ── Hex-print helpers ───────────────────────────────────────────────────── */
static const char hex_chars[16] = "0123456789abcdef";

static void print_hex(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        char tmp[2];
        tmp[0] = hex_chars[(buf[i] >> 4) & 0xF];
        tmp[1] = hex_chars[ buf[i]        & 0xF];
        /* htif_puts is char-by-char internally; pass 2-char NUL-terminated buf */
        char s[3] = { tmp[0], tmp[1], '\0' };
        hqc_htif_puts(s);
    }
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    int failures = 0;

    for (int i = 0; i < KAT_COUNT; i++) {
        /* Re-seed the DRBG for this vector */
        randombytes_init(kat_seeds[i]);

        /* KeyGen */
        if (crypto_kem_keypair(pk, sk) != 0) {
            hqc_htif_puts("FAIL keypair at ");
            hqc_htif_putu64((uint64_t)i);
            hqc_htif_puts("\n");
            failures++;
            continue;
        }

        /* Encaps */
        if (crypto_kem_enc(ct, ss_enc, pk) != 0) {
            hqc_htif_puts("FAIL enc at ");
            hqc_htif_putu64((uint64_t)i);
            hqc_htif_puts("\n");
            failures++;
            continue;
        }

        /* Decaps */
        if (crypto_kem_dec(ss_dec, ct, sk) != 0) {
            hqc_htif_puts("FAIL dec at ");
            hqc_htif_putu64((uint64_t)i);
            hqc_htif_puts("\n");
            failures++;
            continue;
        }

        /* Shared-secret consistency check */
        if (memcmp(ss_enc, ss_dec, CRYPTO_BYTES) != 0) {
            hqc_htif_puts("FAIL ss_mismatch at ");
            hqc_htif_putu64((uint64_t)i);
            hqc_htif_puts("\n");
            failures++;
            continue;
        }

        /* Emit shared secret for external diff against .rsp */
        hqc_htif_puts("ss: ");
        print_hex(ss_enc, CRYPTO_BYTES);
        hqc_htif_puts("\n");
    }

    /* Summary line */
    int passed = KAT_COUNT - failures;
    if (failures == 0) {
        hqc_htif_puts("PASS ");
        hqc_htif_putu64(KAT_COUNT);
        hqc_htif_puts("/");
        hqc_htif_putu64(KAT_COUNT);
        hqc_htif_puts("\n");
        hqc_htif_exit(0);
    } else {
        hqc_htif_puts("FAIL ");
        hqc_htif_putu64((uint64_t)failures);
        hqc_htif_puts(" failures (");
        hqc_htif_putu64((uint64_t)passed);
        hqc_htif_puts("/");
        hqc_htif_putu64(KAT_COUNT);
        hqc_htif_puts(" passed)\n");
        hqc_htif_exit(1);
    }

    return 0;  /* unreachable — hqc_htif_exit never returns */
}
