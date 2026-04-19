/* include/api.h
 *
 * NIST KEM API — thin wrapper that routes to the level-specific functions
 * compiled via the security-level-parameterised CMake targets.
 *
 * The three function names are defined by the NIST PQC submission API:
 *   crypto_kem_keypair  — generate a public/secret key pair
 *   crypto_kem_enc      — encapsulate a shared secret with a public key
 *   crypto_kem_dec      — decapsulate a ciphertext with the secret key
 *
 * Key/ciphertext sizes are taken from parameters.h which is always included
 * before api.h (enforced by the CMake include-directory order in
 * cmake/hqc_core.cmake — the level-specific parameters.h directory appears
 * first on the include path).
 */

#ifndef HQC_API_H
#define HQC_API_H

#include <stdint.h>
#include "parameters.h"

/* Byte lengths defined in the level-specific parameters.h:
 *   CRYPTO_PUBLICKEYBYTES   public key size in bytes
 *   CRYPTO_SECRETKEYBYTES   secret key size in bytes
 *   CRYPTO_CIPHERTEXTBYTES  ciphertext size in bytes
 *   CRYPTO_BYTES            shared secret size in bytes (= 64 for all levels)
 */

/*
 * crypto_kem_keypair
 * Generate a KEM key pair.
 *
 * pk[CRYPTO_PUBLICKEYBYTES]  ← public key output
 * sk[CRYPTO_SECRETKEYBYTES]  ← secret key output
 * Returns 0 on success.
 */
int crypto_kem_keypair(uint8_t *pk, uint8_t *sk);

/*
 * crypto_kem_enc
 * Encapsulate: generate a shared secret and its ciphertext.
 *
 * ct[CRYPTO_CIPHERTEXTBYTES] ← ciphertext output
 * ss[CRYPTO_BYTES]           ← shared secret output
 * pk[CRYPTO_PUBLICKEYBYTES]  ← recipient's public key
 * Returns 0 on success.
 */
int crypto_kem_enc(uint8_t *ct, uint8_t *ss, const uint8_t *pk);

/*
 * crypto_kem_dec
 * Decapsulate: recover the shared secret from a ciphertext.
 *
 * ss[CRYPTO_BYTES]           ← shared secret output
 * ct[CRYPTO_CIPHERTEXTBYTES] ← ciphertext input
 * sk[CRYPTO_SECRETKEYBYTES]  ← recipient's secret key
 * Returns 0 on success, -1 on decapsulation failure.
 */
int crypto_kem_dec(uint8_t *ss, const uint8_t *ct, const uint8_t *sk);

#endif /* HQC_API_H */
