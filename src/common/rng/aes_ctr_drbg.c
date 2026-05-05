/* src/common/rng/aes_ctr_drbg.c
 *
 * NIST AES-256 CTR_DRBG — port of the NIST PQC submission rng.c.
 *
 * Algorithm: NIST SP 800-90A Rev.1 §10.2.1 CTR_DRBG (no df, security strength
 * 256 bits, AES-256, no personalization string, no additional input, no
 * prediction resistance).
 *
 * This must produce bit-identical output to the reference rng.c used to
 * generate the HQC KAT .req / .rsp files.  The reference uses OpenSSL EVP
 * AES-256-ECB; this version uses the local tiny_aes implementation to avoid
 * linking OpenSSL into the bare-metal binary.
 *
 * State:
 *   global AES_XOF_struct DRBG_ctx — initialized once by randombytes_init().
 */

#include "aes_ctr_drbg.h"
#include "tiny_aes.h"
#include "../../../vendor/hqc-ref/src/common/crypto_memset.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

/* ── Module-global DRBG state ─────────────────────────────────────────────── */
static AES_XOF_struct DRBG_ctx;

/* ── Internal helpers ─────────────────────────────────────────────────────── */

/* Increment V as a 128-bit big-endian counter (NIST convention). */
static void inc_counter(uint8_t v[16])
{
    for (int i = 15; i >= 0; i--) {
        if (++v[i]) break;
    }
}

/* CTR_DRBG_Update: absorb provided_data[48] into (key, V).
 * If provided_data is NULL, absorbs all-zero 48-byte string (equivalent).
 * NIST SP 800-90A Rev.1 §10.2.1.2 */
static void ctr_drbg_update(const uint8_t *provided_data,
                             uint8_t key[32],
                             uint8_t v[16])
{
    uint8_t tmp[48];
    uint8_t rk[240];

    aes256_key_expansion(key, rk);

    for (int i = 0; i < 3; i++) {   /* generate 3 × 16 = 48 bytes */
        inc_counter(v);
        aes256_ecb_encrypt(rk, v, tmp + (size_t)i * 16);
    }

    if (provided_data != NULL) {
        for (int i = 0; i < 48; i++)
            tmp[i] ^= provided_data[i];
    }

    memcpy(key, tmp,      32);
    memcpy(v,   tmp + 32, 16);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * randombytes_init — initialize the CTR_DRBG from a 48-byte entropy input.
 *
 * Matches the NIST rng.c prototype:
 *   void randombytes_init(unsigned char *entropy_input,
 *                         unsigned char *personalization_string,
 *                         int security_strength);
 *
 * personalization_string and security_strength are ignored (NIST KAT submissions
 * always pass NULL and 256 respectively; they have no effect in this no-df mode).
 */
void randombytes_init(const uint8_t seed[48])
{
    memset_zero(DRBG_ctx.key, 32);
    memset_zero(DRBG_ctx.v,   16);
    ctr_drbg_update(seed, DRBG_ctx.key, DRBG_ctx.v);
}

/*
 * randombytes — generate `len` pseudorandom bytes.
 *
 * Returns 0 on success (always, on bare metal; no catastrophic failure mode).
 *
 * NIST SP 800-90A Rev.1 §10.2.1.5.2  (generate without additional input,
 * requestedNumberOfBits <= 2^19).
 */
int randombytes(uint8_t *out, size_t len)
{
    uint8_t rk[240];
    uint8_t block[16];
    size_t  remaining = len;

    aes256_key_expansion(DRBG_ctx.key, rk);

    while (remaining > 0) {
        inc_counter(DRBG_ctx.v);
        aes256_ecb_encrypt(rk, DRBG_ctx.v, block);

        size_t n = (remaining < 16) ? remaining : 16;
        memcpy(out, block, n);
        out       += n;
        remaining -= n;
    }

    /* CTR_DRBG_Update with null additional_input after generation */
    ctr_drbg_update(NULL, DRBG_ctx.key, DRBG_ctx.v);
    return 0;
}
