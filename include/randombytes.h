/* include/randombytes.h
 *
 * randombytes() / randombytes_init() declarations.
 *
 * The implementation is selected by the build mode:
 *   HQC_KAT_MODE=1   → AES-CTR DRBG (src/common/rng/aes_ctr_drbg.c)
 *   HQC_BENCH_MODE=1 → fixed-pattern fill (src/common/randombytes.c)
 *
 * In HQC_KAT_MODE, randombytes_init() accepts the 48-byte entropy input that
 * seeds the CTR_DRBG.  In HQC_BENCH_MODE it is a no-op; call it anyway for
 * source compatibility.
 */

#ifndef HQC_RANDOMBYTES_H
#define HQC_RANDOMBYTES_H

#include <stdint.h>
#include <stddef.h>

/*
 * randombytes_init — seed the PRNG.
 *
 * KAT mode : seed[48] is the 48-byte entropy input from the .req file.
 * Bench mode: no-op.
 */
void randombytes_init(const uint8_t seed[48]);

/*
 * randombytes — fill out[0..len-1] with pseudorandom bytes.
 * Returns 0 on success (never fails on bare metal).
 */
int randombytes(uint8_t *out, size_t len);

#endif /* HQC_RANDOMBYTES_H */
