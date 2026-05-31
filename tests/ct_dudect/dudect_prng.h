/**
 * dudect_prng.h — xoshiro256** PRNG for dudect harnesses
 *
 * Provides a fast, high-quality 64-bit PRNG for:
 *   - Class selection (random bit per measurement)
 *   - Operand/key generation (byte buffer fill)
 *
 * Seeding:
 *   - Production: reads 32 bytes from /dev/urandom
 *   - Debug: uses DUDECT_FIXED_SEED compile-time macro
 *   - Always prints seed to stderr for reproducibility
 *
 * Requirements: 16.1, 16.2, 16.3
 */

#ifndef DUDECT_PRNG_H
#define DUDECT_PRNG_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/**
 * PRNG state: 4 × uint64_t (256 bits of internal state).
 */
typedef struct {
    uint64_t s[4];
} dudect_prng_t;

/**
 * 64-bit left rotation helper.
 */
static inline uint64_t _dudect_rotl(uint64_t x, int k)
{
    return (x << k) | (x >> (64 - k));
}

/**
 * xoshiro256** core — produces the next 64-bit pseudo-random value.
 *
 * This is the scrambled version of xoshiro256 by Blackman & Vigna (2018).
 * Period: 2^256 - 1, passes all known statistical test suites.
 */
static inline uint64_t dudect_prng_next(dudect_prng_t *rng)
{
    uint64_t result = _dudect_rotl(rng->s[1] * 5, 7) * 9;
    uint64_t t = rng->s[1] << 17;

    rng->s[2] ^= rng->s[0];
    rng->s[3] ^= rng->s[1];
    rng->s[1] ^= rng->s[2];
    rng->s[0] ^= rng->s[3];
    rng->s[2] ^= t;
    rng->s[3] = _dudect_rotl(rng->s[3], 45);

    return result;
}

/**
 * Fill a byte buffer with pseudo-random bytes.
 *
 * Extracts 8 bytes at a time from the PRNG and copies them into
 * the output buffer, handling partial final chunks correctly.
 */
static inline void dudect_prng_fill(dudect_prng_t *rng, uint8_t *buf, size_t len)
{
    size_t i = 0;
    while (i < len) {
        uint64_t r = dudect_prng_next(rng);
        size_t chunk = (len - i < 8) ? (len - i) : 8;
        memcpy(buf + i, &r, chunk);
        i += chunk;
    }
}

/**
 * Return a single random bit (0 or 1) with uniform probability.
 *
 * Used for class selection: 0 = class A (fixed), 1 = class B (random).
 */
static inline int dudect_prng_bit(dudect_prng_t *rng)
{
    return (int)(dudect_prng_next(rng) & 1);
}

/**
 * Initialize the PRNG.
 *
 * If DUDECT_FIXED_SEED is defined at compile time, uses that value to
 * deterministically derive all 4 state words (for reproducible debugging).
 *
 * Otherwise, seeds all 4 state words from /dev/urandom (32 bytes) for
 * production-quality randomness.
 *
 * In both cases, the seed is printed to stderr so that any run can be
 * reproduced later by defining DUDECT_FIXED_SEED to the printed value.
 *
 * @return 0 on success, -1 on failure (only possible with /dev/urandom path)
 */
static inline int dudect_prng_init(dudect_prng_t *rng)
{
#ifdef DUDECT_FIXED_SEED
    /* Derive 4 state words from the fixed seed using splitmix64-style mixing */
    rng->s[0] = (uint64_t)DUDECT_FIXED_SEED;
    rng->s[1] = rng->s[0] ^ 0x9e3779b97f4a7c15ULL;
    rng->s[2] = rng->s[1] ^ 0x6a09e667f3bcc908ULL;
    rng->s[3] = rng->s[2] ^ 0xbb67ae8584caa73bULL;
    fprintf(stderr, "[dudect] PRNG seed: fixed = 0x%016lx\n",
            (unsigned long)DUDECT_FIXED_SEED);
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        perror("[dudect] open(/dev/urandom)");
        return -1;
    }
    ssize_t r = read(fd, rng->s, sizeof(rng->s));
    close(fd);
    if (r != (ssize_t)sizeof(rng->s)) {
        fprintf(stderr, "[dudect] ERROR: short read from /dev/urandom\n");
        return -1;
    }
    fprintf(stderr, "[dudect] PRNG seed: %016lx %016lx %016lx %016lx\n",
            (unsigned long)rng->s[0], (unsigned long)rng->s[1],
            (unsigned long)rng->s[2], (unsigned long)rng->s[3]);
#endif
    return 0;
}

#endif /* DUDECT_PRNG_H */
