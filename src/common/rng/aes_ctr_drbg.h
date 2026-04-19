/* src/common/rng/aes_ctr_drbg.h
 *
 * NIST AES-256 CTR_DRBG interface.
 *
 * This is the deterministic random-byte generator used by NIST PQC KAT
 * submission packages (rng.h / rng.c in the reference implementation).
 * The interface and internal state layout MUST match the NIST specification
 * exactly; any deviation will cause KAT vectors to diverge.
 *
 * State: (key[32], V[16]) — 256-bit key and 128-bit counter block.
 * No personalization string, no prediction resistance, no reseeding.
 *
 * Reference: NIST SP 800-90A Rev.1, Section 10.2.1.
 */

#ifndef HQC_AES_CTR_DRBG_H
#define HQC_AES_CTR_DRBG_H

#include <stdint.h>
#include <stddef.h>

/* DRBG internal state — opaque outside of this module. */
typedef struct {
    uint8_t key[32];  /* AES-256 key         */
    uint8_t v[16];    /* 128-bit counter block (big-endian) */
} AES_XOF_struct;

/* Initialize the DRBG from a 48-byte seed.
 * seed[0..31]  → initial key
 * seed[32..47] → initial V (leftmost 16 bytes; remaining bytes discarded) */
void randombytes_init(const uint8_t seed[48]);

/* Fill out[0..len-1] with deterministic pseudorandom bytes.
 * Updates the DRBG state in-place (key and V advance). */
int  randombytes(uint8_t *out, size_t len);

#endif /* HQC_AES_CTR_DRBG_H */
