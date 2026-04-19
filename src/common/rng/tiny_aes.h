/* src/common/rng/tiny_aes.h
 *
 * Minimal AES-256 ECB interface used by the KAT AES-CTR DRBG.
 *
 * Only AES-256 encryption is needed.  No key schedule caching is required
 * because the DRBG always follows KeyExpansion immediately with a single block
 * encryption.
 */

#ifndef HQC_TINY_AES_H
#define HQC_TINY_AES_H

#include <stdint.h>

/* Perform AES-256 key expansion.
 * key[32] → expanded form stored in round_key[240]. */
void aes256_key_expansion(const uint8_t key[32], uint8_t round_key[240]);

/* Encrypt one 16-byte block in ECB mode.
 * round_key must have been populated by aes256_key_expansion().
 * Output is written into out[16]; in[16] and out[16] may NOT alias. */
void aes256_ecb_encrypt(const uint8_t round_key[240],
                        const uint8_t in[16],
                        uint8_t       out[16]);

#endif /* HQC_TINY_AES_H */
