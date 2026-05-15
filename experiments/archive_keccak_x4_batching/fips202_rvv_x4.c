/* src/keccak/rvv/fips202_rvv_x4.c
 *
 * fips202.h drop-in backed by the PQRV CHES 2025 hybrid-x4 assembly permute.
 *
 * The PQRV assembly (fips202_rv64imv_hybrid_x4.S or fips202_rv64imbv_hybrid_x4.S)
 * exports KeccakF1600_StatePermute_RV64V_4x, which expects all 4 states packed
 * in the PQRV interleaved layout (per the S(j,i) macro in fips202x.h):
 *
 *   j=0: s[2*i]    for i=0..24  (interleaved, even indices)
 *   j=1: s[2*i+1]  for i=0..24  (interleaved, odd indices)
 *   j=2: s[50+i]   for i=0..24  (contiguous block)
 *   j=3: s[75+i]   for i=0..24  (contiguous block)
 *
 * With uint64_t states[4][25], the j=2 slot maps exactly to states[2][0..24]
 * (at flat offset 50 from &states[0][0]), giving a simple memcpy wrapper.
 *
 * We declare the assembly symbol directly to avoid pulling in fips202x.h,
 * which requires PQRV-specific types (keccak_state) not present in our
 * vendor/hqc-ref/lib/fips202/fips202.h.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "fips202.h"   /* public fips202 API types (from vendor/hqc-ref/lib/fips202) */

/* Assembly symbol exported by both imv and imbv .S files.                  */
extern void KeccakF1600_StatePermute_RV64V_4x(uint64_t *state);

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 1 — Single-state Keccak-f[1600] via PQRV x4 assembly
 * ═══════════════════════════════════════════════════════════════════════════ */

/*
 * KeccakF1600_StatePermute — wrap the 4-way vendor permute for a single state.
 *
 * The PQRV function KeccakF1600x4_StatePermute(s) processes all four instances
 * packed into the interleaved buffer.  We use the j=2 slot (contiguous at
 * s[50..74]) because it allows a plain memcpy rather than element-wise
 * interleaving.  The three remaining slots (j=0,1,3) are zeroed and their
 * outputs are discarded.
 */
static void KeccakF1600_StatePermute(uint64_t *state)
{
    /* Allocate the full 4-instance interleaved buffer on the stack. */
    uint64_t states[4][25];
    memset(states, 0, sizeof(states));

    /*
     * Place the real state in the j=2 slot.
     * &states[0][0] is the base pointer for the entire 100-word block;
     * states[2][i] == (&states[0][0])[50 + i], which is exactly s[50+i]
     * as expected by the PQRV S(2,i) macro.
     */
    memcpy(states[2], state, 25 * sizeof(uint64_t));

    /* Invoke the vendor 4-way permute with the single base pointer. */
    KeccakF1600_StatePermute_RV64V_4x(&states[0][0]);

    /* Extract result from j=2 slot. */
    memcpy(state, states[2], 25 * sizeof(uint64_t));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 2 — Helper: load/store 64-bit words in little-endian order
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint64_t load64_le(const uint8_t *x)
{
    uint64_t r = 0;
    for (size_t i = 0; i < 8; i++)
        r |= (uint64_t)x[i] << (8 * i);
    return r;
}

static inline void store64_le(uint8_t *x, uint64_t u)
{
    for (size_t i = 0; i < 8; i++)
        x[i] = (uint8_t)(u >> (8 * i));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 3 — Scalar absorb / squeeze helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static void keccak_absorb(uint64_t *s, uint32_t r, const uint8_t *m,
                          size_t mlen, uint8_t p)
{
    size_t i;
    uint8_t t[200];

    for (i = 0; i < 25; i++)
        s[i] = 0;

    while (mlen >= r) {
        for (i = 0; i < r / 8; i++)
            s[i] ^= load64_le(m + 8 * i);
        KeccakF1600_StatePermute(s);
        mlen -= r;
        m += r;
    }

    for (i = 0; i < r; i++)
        t[i] = 0;
    for (i = 0; i < mlen; i++)
        t[i] = m[i];
    t[i] = p;
    t[r - 1] |= 128;
    for (i = 0; i < r / 8; i++)
        s[i] ^= load64_le(t + 8 * i);
}

static void keccak_squeezeblocks(uint8_t *h, size_t nblocks,
                                 uint64_t *s, uint32_t r)
{
    while (nblocks > 0) {
        KeccakF1600_StatePermute(s);
        for (size_t i = 0; i < (r >> 3); i++)
            store64_le(h + 8 * i, s[i]);
        h += r;
        nblocks--;
    }
}

static void keccak_inc_init(uint64_t *s_inc)
{
    for (size_t i = 0; i < 25; i++)
        s_inc[i] = 0;
    s_inc[25] = 0;
}

static void keccak_inc_absorb(uint64_t *s_inc, uint32_t r,
                               const uint8_t *m, size_t mlen)
{
    size_t i;
    while (mlen + s_inc[25] >= r) {
        for (i = 0; i < r - (uint32_t)s_inc[25]; i++)
            s_inc[(s_inc[25] + i) >> 3] ^=
                (uint64_t)m[i] << (8 * ((s_inc[25] + i) & 0x07));
        mlen -= (size_t)(r - s_inc[25]);
        m    += r - s_inc[25];
        s_inc[25] = 0;
        KeccakF1600_StatePermute(s_inc);
    }
    for (i = 0; i < mlen; i++)
        s_inc[(s_inc[25] + i) >> 3] ^=
            (uint64_t)m[i] << (8 * ((s_inc[25] + i) & 0x07));
    s_inc[25] += mlen;
}

static void keccak_inc_finalize(uint64_t *s_inc, uint32_t r, uint8_t p)
{
    s_inc[s_inc[25] >> 3]    ^= (uint64_t)p   << (8 * (s_inc[25] & 0x07));
    s_inc[(r - 1) >> 3]      ^= (uint64_t)128 << (8 * ((r - 1) & 0x07));
    s_inc[25] = 0;
}

static void keccak_inc_squeeze(uint8_t *h, size_t outlen,
                                uint64_t *s_inc, uint32_t r)
{
    size_t i;
    for (i = 0; i < outlen && i < s_inc[25]; i++)
        h[i] = (uint8_t)(s_inc[(r - s_inc[25] + i) >> 3]
                         >> (8 * ((r - s_inc[25] + i) & 0x07)));
    h      += i;
    outlen -= i;
    s_inc[25] -= i;

    while (outlen > 0) {
        KeccakF1600_StatePermute(s_inc);
        for (i = 0; i < outlen && i < r; i++)
            h[i] = (uint8_t)(s_inc[i >> 3] >> (8 * (i & 0x07)));
        h      += i;
        outlen -= i;
        s_inc[25] = r - i;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 4 — Public single-stream API (drop-in for fips202.h)
 * ═══════════════════════════════════════════════════════════════════════════ */

void shake128_absorb(shake128ctx *state, const uint8_t *input, size_t inlen)
    { keccak_absorb(state->ctx, SHAKE128_RATE, input, inlen, 0x1F); }

void shake128_squeezeblocks(uint8_t *output, size_t nblocks, shake128ctx *state)
    { keccak_squeezeblocks(output, nblocks, state->ctx, SHAKE128_RATE); }

void shake128_inc_init(shake128incctx *state)
    { keccak_inc_init(state->ctx); }
void shake128_inc_absorb(shake128incctx *state, const uint8_t *input, size_t inlen)
    { keccak_inc_absorb(state->ctx, SHAKE128_RATE, input, inlen); }
void shake128_inc_finalize(shake128incctx *state)
    { keccak_inc_finalize(state->ctx, SHAKE128_RATE, 0x1F); }
void shake128_inc_squeeze(uint8_t *output, size_t outlen, shake128incctx *state)
    { keccak_inc_squeeze(output, outlen, state->ctx, SHAKE128_RATE); }

void shake256_absorb(shake256ctx *state, const uint8_t *input, size_t inlen)
    { keccak_absorb(state->ctx, SHAKE256_RATE, input, inlen, 0x1F); }

void shake256_squeezeblocks(uint8_t *output, size_t nblocks, shake256ctx *state)
    { keccak_squeezeblocks(output, nblocks, state->ctx, SHAKE256_RATE); }

void shake256_inc_init(shake256incctx *state)
    { keccak_inc_init(state->ctx); }
void shake256_inc_absorb(shake256incctx *state, const uint8_t *input, size_t inlen)
    { keccak_inc_absorb(state->ctx, SHAKE256_RATE, input, inlen); }
void shake256_inc_finalize(shake256incctx *state)
    { keccak_inc_finalize(state->ctx, SHAKE256_RATE, 0x1F); }
void shake256_inc_squeeze(uint8_t *output, size_t outlen, shake256incctx *state)
    { keccak_inc_squeeze(output, outlen, state->ctx, SHAKE256_RATE); }

void shake128(uint8_t *output, size_t outlen,
              const uint8_t *input, size_t inlen)
{
    size_t nblocks = outlen / SHAKE128_RATE;
    uint8_t t[SHAKE128_RATE];
    shake128ctx s;
    shake128_absorb(&s, input, inlen);
    shake128_squeezeblocks(output, nblocks, &s);
    output += nblocks * SHAKE128_RATE;
    outlen -= nblocks * SHAKE128_RATE;
    if (outlen) {
        shake128_squeezeblocks(t, 1, &s);
        for (size_t i = 0; i < outlen; i++)
            output[i] = t[i];
    }
}

void shake256(uint8_t *output, size_t outlen,
              const uint8_t *input, size_t inlen)
{
    size_t nblocks = outlen / SHAKE256_RATE;
    uint8_t t[SHAKE256_RATE];
    shake256ctx s;
    shake256_absorb(&s, input, inlen);
    shake256_squeezeblocks(output, nblocks, &s);
    output += nblocks * SHAKE256_RATE;
    outlen -= nblocks * SHAKE256_RATE;
    if (outlen) {
        shake256_squeezeblocks(t, 1, &s);
        for (size_t i = 0; i < outlen; i++)
            output[i] = t[i];
    }
}

void sha3_256_inc_init(sha3_256incctx *state)
    { keccak_inc_init(state->ctx); }
void sha3_256_inc_absorb(sha3_256incctx *state, const uint8_t *input, size_t inlen)
    { keccak_inc_absorb(state->ctx, SHA3_256_RATE, input, inlen); }
void sha3_256_inc_finalize(uint8_t *output, sha3_256incctx *state)
{
    uint8_t t[SHA3_256_RATE];
    keccak_inc_finalize(state->ctx, SHA3_256_RATE, 0x06);
    keccak_squeezeblocks(t, 1, state->ctx, SHA3_256_RATE);
    for (size_t i = 0; i < 32; i++)
        output[i] = t[i];
}
void sha3_256(uint8_t *output, const uint8_t *input, size_t inlen)
{
    uint64_t s[25];
    uint8_t t[SHA3_256_RATE];
    keccak_absorb(s, SHA3_256_RATE, input, inlen, 0x06);
    keccak_squeezeblocks(t, 1, s, SHA3_256_RATE);
    for (size_t i = 0; i < 32; i++)
        output[i] = t[i];
}

void sha3_384_inc_init(sha3_384incctx *state)
    { keccak_inc_init(state->ctx); }
void sha3_384_inc_absorb(sha3_384incctx *state, const uint8_t *input, size_t inlen)
    { keccak_inc_absorb(state->ctx, SHA3_384_RATE, input, inlen); }
void sha3_384_inc_finalize(uint8_t *output, sha3_384incctx *state)
{
    uint8_t t[SHA3_384_RATE];
    keccak_inc_finalize(state->ctx, SHA3_384_RATE, 0x06);
    keccak_squeezeblocks(t, 1, state->ctx, SHA3_384_RATE);
    for (size_t i = 0; i < 48; i++)
        output[i] = t[i];
}
void sha3_384(uint8_t *output, const uint8_t *input, size_t inlen)
{
    uint64_t s[25];
    uint8_t t[SHA3_384_RATE];
    keccak_absorb(s, SHA3_384_RATE, input, inlen, 0x06);
    keccak_squeezeblocks(t, 1, s, SHA3_384_RATE);
    for (size_t i = 0; i < 48; i++)
        output[i] = t[i];
}

void sha3_512_inc_init(sha3_512incctx *state)
    { keccak_inc_init(state->ctx); }
void sha3_512_inc_absorb(sha3_512incctx *state, const uint8_t *input, size_t inlen)
    { keccak_inc_absorb(state->ctx, SHA3_512_RATE, input, inlen); }
void sha3_512_inc_finalize(uint8_t *output, sha3_512incctx *state)
{
    uint8_t t[SHA3_512_RATE];
    keccak_inc_finalize(state->ctx, SHA3_512_RATE, 0x06);
    keccak_squeezeblocks(t, 1, state->ctx, SHA3_512_RATE);
    for (size_t i = 0; i < 64; i++)
        output[i] = t[i];
}
void sha3_512(uint8_t *output, const uint8_t *input, size_t inlen)
{
    uint64_t s[25];
    uint8_t t[SHA3_512_RATE];
    keccak_absorb(s, SHA3_512_RATE, input, inlen, 0x06);
    keccak_squeezeblocks(t, 1, s, SHA3_512_RATE);
    for (size_t i = 0; i < 64; i++)
        output[i] = t[i];
}
