/* src/keccak/fips202_zvbb_x2.c
 *
 * Track C2 (EXP-keccak-x2-soa): consolidated self-contained Keccak backend.
 *
 * This translation unit embeds both:
 *   1) single-stream Zvbb Keccak-f[1600] + full fips202.h drop-in API
 *   2) 2-way SoA wrappers (shake256_x2_*) used by HQC keygen fast path
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>

#include "fips202.h"

/* ── Embedded single-stream Zvbb core (formerly fips202_zvbb_core.c) ─────── */
/* ═══════════════════════════════════════════════════════════════════════════
 * Section 1 — LMUL=8 vrgather Keccak-f[1600]
 * ═══════════════════════════════════════════════════════════════════════════ */

#define NROUNDS 24
#define ROL(a, n) (((a) << (n)) | ((a) >> (64 - (n))))

static const uint64_t KeccakF_RoundConstants[NROUNDS] = {
    0x0000000000000001ULL, 0x0000000000008082ULL,
    0x800000000000808aULL, 0x8000000080008000ULL,
    0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008aULL, 0x0000000000000088ULL,
    0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL,
    0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL,
    0x8000000080008081ULL, 0x8000000000008080ULL,
    0x0000000080000001ULL, 0x8000000080008008ULL
};

/*
 * Pi source indices: new[j] = old[pi_src[j]]
 *
 * From Pi: B[y, (2x+3y)%5] = A[x,y], flat i=x+5y → dest = y + 5*((2x+3y)%5)
 * pi_src[dest] = src_i  (inverse mapping)
 *
 * pi_src[j] for j = 0..24:
 *   j:   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
 *   src: 0  6 12 18 24  3  9 10 16 22  1  7 13 19 20  4  5 11 17 23  2  8 14 15 21
 */
static const uint64_t pi_src[25] = {
     0,  6, 12, 18, 24,
     3,  9, 10, 16, 22,
     1,  7, 13, 19, 20,
     4,  5, 11, 17, 23,
     2,  8, 14, 15, 21
};

/*
 * ROR amounts for each Pi-destination lane (Rho applied after Pi).
 * ror_amounts[j] = (64 - rho_src[pi_src[j]]) % 64
 *
 * rho_src (flat order, i = x + 5y):
 *   i:  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
 *   ρ:  0  1 62 28 27 36 44  6 55 20  3 10 43 25 39 41 45 15 21  8 18  2 61 56 14
 *
 * ror_amounts[j] for j = 0..24:
 *   j:   0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24
 *   ror: 0 20 21 43 50 36 44 61 19  3 63 58 39 56 46 37 28 54 49  8  2  9 25 23 62
 */
static const uint64_t ror_amounts[25] = {
     0, 20, 21, 43, 50,
    36, 44, 61, 19,  3,
    63, 58, 39, 56, 46,
    37, 28, 54, 49,  8,
     2,  9, 25, 23, 62
};

/*
 * Chi x+1 and x+2 indices (cyclic within each row of 5).
 * chi_x1[i] = (i%5 + 1)%5 + 5*(i/5)
 * chi_x2[i] = (i%5 + 2)%5 + 5*(i/5)
 */
static const uint64_t chi_x1[25] = {
     1,  2,  3,  4,  0,
     6,  7,  8,  9,  5,
    11, 12, 13, 14, 10,
    16, 17, 18, 19, 15,
    21, 22, 23, 24, 20
};

static const uint64_t chi_x2[25] = {
     2,  3,  4,  0,  1,
     7,  8,  9,  5,  6,
    12, 13, 14, 10, 11,
    17, 18, 19, 15, 16,
    22, 23, 24, 20, 21
};

/* Theta diffusion index pattern: maps 5-lane D[x] to 25-lane row-major state. */
static const uint64_t theta_col_idx[25] = {
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4
};

static inline uint64_t rol1_zbb(uint64_t x)
{
    uint64_t y;
    __asm__("rori %0, %1, 63" : "=r"(y) : "r"(x));
    return y;
}

/*
 * KeccakF1600_StatePermute — LMUL=8 vrgather implementation.
 *
 * With VLEN=256, SEW=64, LMUL=8: vlmax = 256*8/64 = 32 ≥ 25.
 * vsetvl_e64m8(25) returns 25 on any hardware with VLEN≥256.
 *
 * Register groups used simultaneously (max 4 with LMUL=8 in 32 regs):
 *   group 0 (v0-v7 )  : v0    — main state, live throughout all rounds
 *   group 1 (v8-v15)  : t/vB  — round temporaries (reused each step)
 *   group 2 (v16-v23) : vC    — round temporary (Chi second gather)
 *   group 3 (v24-v31) : free / compiler allocation for extra temporaries
 *
 * The five constant index tables (idx_pi, idx_ror, idx_x1, idx_x2, idx_theta)
 * are
 * loaded once before the round loop and held as C variables.  The compiler
 * keeps them in whichever groups are free or spills them to the vector
 * stack; either way the 4 × 24 = 96 repeated vle64.v instructions inside
 * the loop are eliminated, saving ~364 K instret per permutation call site.
 */
static void KeccakF1600_StatePermute(uint64_t *state)
{
    size_t vl = __riscv_vsetvl_e64m8(25);

    vuint64m8_t v0 = __riscv_vle64_v_u64m8(state, vl);

    /* ── Pre-load constant index tables (once per permutation call) ─────── *
     * Eliminates 4 × 24 = 96 vle64.v instructions from the hot round loop. */
    vuint64m8_t idx_pi  = __riscv_vle64_v_u64m8(pi_src,      vl);
    vuint64m8_t idx_ror = __riscv_vle64_v_u64m8(ror_amounts,  vl);
    vuint64m8_t idx_x1  = __riscv_vle64_v_u64m8(chi_x1,       vl);
    vuint64m8_t idx_x2  = __riscv_vle64_v_u64m8(chi_x2,       vl);
    vuint64m8_t idx_theta = __riscv_vle64_v_u64m8(theta_col_idx, vl);

    for (int round = 0; round < NROUNDS; round++) {

        /* ── Theta: in-register parity extraction + diffusion ───────────── */
        {
            size_t vl5 = __riscv_vsetvl_e64m8(5);

            vuint64m8_t vC = v0;
            vC = __riscv_vxor_vv_u64m8(vC, __riscv_vslidedown_vx_u64m8(v0, 5,  vl5), vl5);
            vC = __riscv_vxor_vv_u64m8(vC, __riscv_vslidedown_vx_u64m8(v0, 10, vl5), vl5);
            vC = __riscv_vxor_vv_u64m8(vC, __riscv_vslidedown_vx_u64m8(v0, 15, vl5), vl5);
            vC = __riscv_vxor_vv_u64m8(vC, __riscv_vslidedown_vx_u64m8(v0, 20, vl5), vl5);

            uint64_t C0 = __riscv_vmv_x_s_u64m8_u64(vC);
            vC = __riscv_vslidedown_vx_u64m8(vC, 1, vl5);
            uint64_t C1 = __riscv_vmv_x_s_u64m8_u64(vC);
            vC = __riscv_vslidedown_vx_u64m8(vC, 1, vl5);
            uint64_t C2 = __riscv_vmv_x_s_u64m8_u64(vC);
            vC = __riscv_vslidedown_vx_u64m8(vC, 1, vl5);
            uint64_t C3 = __riscv_vmv_x_s_u64m8_u64(vC);
            vC = __riscv_vslidedown_vx_u64m8(vC, 1, vl5);
            uint64_t C4 = __riscv_vmv_x_s_u64m8_u64(vC);

            uint64_t D0 = C4 ^ rol1_zbb(C1);
            uint64_t D1 = C0 ^ rol1_zbb(C2);
            uint64_t D2 = C1 ^ rol1_zbb(C3);
            uint64_t D3 = C2 ^ rol1_zbb(C4);
            uint64_t D4 = C3 ^ rol1_zbb(C0);

            vuint64m8_t vD = __riscv_vmv_v_x_u64m8(D4, vl5);
            vD = __riscv_vslide1up_vx_u64m8(vD, D3, vl5);
            vD = __riscv_vslide1up_vx_u64m8(vD, D2, vl5);
            vD = __riscv_vslide1up_vx_u64m8(vD, D1, vl5);
            vD = __riscv_vslide1up_vx_u64m8(vD, D0, vl5);

            vuint64m8_t vDfull = __riscv_vrgather_vv_u64m8(vD, idx_theta, vl);
            v0 = __riscv_vxor_vv_u64m8(v0, vDfull, vl);

            vl = __riscv_vsetvl_e64m8(25);
        }

        /* ── Rho + Pi: vrgather (Pi) then vror (Rho) ────────────────────── */
        {
            vuint64m8_t t = __riscv_vrgather_vv_u64m8(v0, idx_pi, vl);
            v0 = __riscv_vror_vv_u64m8(t, idx_ror, vl);
        }

        /* ── Chi: vrgather + vandn ──────────────────────────────────────── */
        {
            vuint64m8_t vB = __riscv_vrgather_vv_u64m8(v0, idx_x1, vl);
            vuint64m8_t vC = __riscv_vrgather_vv_u64m8(v0, idx_x2, vl);
            /* vandn(op1,op2)=op1&~op2, so vandn(vC,vB)=vC&~vB=~vB&vC=~A[x+1]&A[x+2] */
            v0 = __riscv_vxor_vv_u64m8(
                     v0,
                     __riscv_vandn_vv_u64m8(vC, vB, vl),
                     vl);
        }

        /* ── Iota: XOR round constant into lane 0 only ──────────────────── */
        {
            size_t vl1 = __riscv_vsetvl_e64m8(1);
            v0  = __riscv_vxor_vx_u64m8(v0, KeccakF_RoundConstants[round], vl1);
            vl  = __riscv_vsetvl_e64m8(25);
        }
    }

    __riscv_vse64_v_u64m8(state, v0, vl);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 2 — Helper: load/store 64-bit words in little-endian order
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint64_t load64_le(const uint8_t *x)
{
    uint64_t r = 0;
    for (size_t i = 0; i < 8; i++) {
        r |= (uint64_t)x[i] << (8 * i);
    }
    return r;
}

static inline void store64_le(uint8_t *x, uint64_t u)
{
    for (size_t i = 0; i < 8; i++) {
        x[i] = (uint8_t)(u >> (8 * i));
    }
}

/* XOR len bytes into the Keccak state at byte offset pos. */
static inline void keccak_xor_bytes(uint64_t *s_inc,
                                    size_t pos,
                                    const uint8_t *in,
                                    size_t len)
{
    size_t i = 0;

    while (i < len && ((pos + i) & 0x07)) {
        size_t p = pos + i;
        s_inc[p >> 3] ^= (uint64_t)in[i] << (8 * (p & 0x07));
        i++;
    }

    if (i < len) {
        size_t word = (pos + i) >> 3;
        while (i + 8 <= len) {
            s_inc[word++] ^= load64_le(in + i);
            i += 8;
        }
    }

    while (i < len) {
        size_t p = pos + i;
        s_inc[p >> 3] ^= (uint64_t)in[i] << (8 * (p & 0x07));
        i++;
    }
}

/* Extract len bytes from the Keccak state starting at byte offset pos. */
static inline void keccak_extract_bytes(const uint64_t *s_inc,
                                        size_t pos,
                                        uint8_t *out,
                                        size_t len)
{
    size_t i = 0;

    while (i < len && ((pos + i) & 0x07)) {
        size_t p = pos + i;
        out[i] = (uint8_t)(s_inc[p >> 3] >> (8 * (p & 0x07)));
        i++;
    }

    if (i < len) {
        size_t word = (pos + i) >> 3;
        while (i + 8 <= len) {
            store64_le(out + i, s_inc[word++]);
            i += 8;
        }
    }

    while (i < len) {
        size_t p = pos + i;
        out[i] = (uint8_t)(s_inc[p >> 3] >> (8 * (p & 0x07)));
        i++;
    }
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
    while (mlen + s_inc[25] >= r) {
        size_t chunk = (size_t)(r - s_inc[25]);
        keccak_xor_bytes(s_inc, (size_t)s_inc[25], m, chunk);
        mlen -= chunk;
        m += chunk;
        s_inc[25] = 0;
        KeccakF1600_StatePermute(s_inc);
    }
    keccak_xor_bytes(s_inc, (size_t)s_inc[25], m, mlen);
    s_inc[25] += mlen;
}

static void keccak_inc_finalize(uint64_t *s_inc, uint32_t r, uint8_t p)
{
    s_inc[s_inc[25] >> 3]  ^= (uint64_t)p   << (8 * (s_inc[25] & 0x07));
    s_inc[(r - 1) >> 3]    ^= (uint64_t)128 << (8 * ((r - 1) & 0x07));
    s_inc[25] = 0;
}

static void keccak_inc_squeeze(uint8_t *h, size_t outlen,
                                uint64_t *s_inc, uint32_t r)
{
    size_t take = outlen < (size_t)s_inc[25] ? outlen : (size_t)s_inc[25];
    if (take > 0) {
        keccak_extract_bytes(s_inc, (size_t)(r - s_inc[25]), h, take);
        h += take;
        outlen -= take;
        s_inc[25] -= take;
    }

    while (outlen > 0) {
        KeccakF1600_StatePermute(s_inc);
        take = outlen < (size_t)r ? outlen : (size_t)r;
        keccak_extract_bytes(s_inc, 0, h, take);
        h += take;
        outlen -= take;
        s_inc[25] = r - take;
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

/* Clang requires fixed-width RVV aliases for arrays of vector values. */
typedef vuint64m1_t vu64m1_fixed_t __attribute__((riscv_rvv_vector_bits(256)));

static inline size_t keccak_x2_setvl(void)
{
    size_t vl;
    __asm__ volatile("vsetivli %0, 2, e64, m1, ta, ma" : "=r"(vl));
    return vl;
}

static inline uint64_t x2_load64_le(const uint8_t *x)
{
    uint64_t r = 0;
    for (size_t i = 0; i < 8; i++) {
        r |= (uint64_t)x[i] << (8 * i);
    }
    return r;
}

static inline void x2_store64_le(uint8_t *x, uint64_t u)
{
    for (size_t i = 0; i < 8; i++) {
        x[i] = (uint8_t)(u >> (8 * i));
    }
}

/* SoA permutation core: state[25], each vector lane is one independent stream. */
static void KeccakF1600_StatePermute_x2(vu64m1_fixed_t *state)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t C[5];
    vu64m1_fixed_t D[5];
    vu64m1_fixed_t B[25];

    for (int round = 0; round < NROUNDS; round++) {
        C[0] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[0], state[5], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[10], state[15], vl), state[20], vl), vl);
        C[1] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[1], state[6], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[11], state[16], vl), state[21], vl), vl);
        C[2] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[2], state[7], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[12], state[17], vl), state[22], vl), vl);
        C[3] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[3], state[8], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[13], state[18], vl), state[23], vl), vl);
        C[4] = __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[4], state[9], vl),
                                     __riscv_vxor_vv_u64m1(__riscv_vxor_vv_u64m1(state[14], state[19], vl), state[24], vl), vl);

        D[0] = __riscv_vxor_vv_u64m1(C[4], __riscv_vror_vx_u64m1(C[1], 63, vl), vl);
        D[1] = __riscv_vxor_vv_u64m1(C[0], __riscv_vror_vx_u64m1(C[2], 63, vl), vl);
        D[2] = __riscv_vxor_vv_u64m1(C[1], __riscv_vror_vx_u64m1(C[3], 63, vl), vl);
        D[3] = __riscv_vxor_vv_u64m1(C[2], __riscv_vror_vx_u64m1(C[4], 63, vl), vl);
        D[4] = __riscv_vxor_vv_u64m1(C[3], __riscv_vror_vx_u64m1(C[0], 63, vl), vl);

        /* ── Theta: apply D[x] across all 5 rows — fully unrolled ──────────── */
        /* Eliminates loop-driven D[] array index reload (5 scalar ld/round).   */
        state[ 0] = __riscv_vxor_vv_u64m1(state[ 0], D[0], vl);
        state[ 5] = __riscv_vxor_vv_u64m1(state[ 5], D[0], vl);
        state[10] = __riscv_vxor_vv_u64m1(state[10], D[0], vl);
        state[15] = __riscv_vxor_vv_u64m1(state[15], D[0], vl);
        state[20] = __riscv_vxor_vv_u64m1(state[20], D[0], vl);
        state[ 1] = __riscv_vxor_vv_u64m1(state[ 1], D[1], vl);
        state[ 6] = __riscv_vxor_vv_u64m1(state[ 6], D[1], vl);
        state[11] = __riscv_vxor_vv_u64m1(state[11], D[1], vl);
        state[16] = __riscv_vxor_vv_u64m1(state[16], D[1], vl);
        state[21] = __riscv_vxor_vv_u64m1(state[21], D[1], vl);
        state[ 2] = __riscv_vxor_vv_u64m1(state[ 2], D[2], vl);
        state[ 7] = __riscv_vxor_vv_u64m1(state[ 7], D[2], vl);
        state[12] = __riscv_vxor_vv_u64m1(state[12], D[2], vl);
        state[17] = __riscv_vxor_vv_u64m1(state[17], D[2], vl);
        state[22] = __riscv_vxor_vv_u64m1(state[22], D[2], vl);
        state[ 3] = __riscv_vxor_vv_u64m1(state[ 3], D[3], vl);
        state[ 8] = __riscv_vxor_vv_u64m1(state[ 8], D[3], vl);
        state[13] = __riscv_vxor_vv_u64m1(state[13], D[3], vl);
        state[18] = __riscv_vxor_vv_u64m1(state[18], D[3], vl);
        state[23] = __riscv_vxor_vv_u64m1(state[23], D[3], vl);
        state[ 4] = __riscv_vxor_vv_u64m1(state[ 4], D[4], vl);
        state[ 9] = __riscv_vxor_vv_u64m1(state[ 9], D[4], vl);
        state[14] = __riscv_vxor_vv_u64m1(state[14], D[4], vl);
        state[19] = __riscv_vxor_vv_u64m1(state[19], D[4], vl);
        state[24] = __riscv_vxor_vv_u64m1(state[24], D[4], vl);

        /* ── Rho + Pi: statically-resolved — compile-time immediates ─────── */
        /* pi_src[] and ror_amounts[] are fully inlined below; the compiler    */
        /* can emit vror.vi (≤31) or vror.vx+li (32-63) with ZERO ld from     */
        /* memory, eliminating 25×2×24 = 1,200 scalar ld per permutation.     */
        B[ 0] = state[ 0];  /* pi_src=0,  ror=0  → identity, no rotation */
        B[ 1] = __riscv_vror_vx_u64m1(state[ 6], 20, vl);
        B[ 2] = __riscv_vror_vx_u64m1(state[12], 21, vl);
        B[ 3] = __riscv_vror_vx_u64m1(state[18], 43, vl);
        B[ 4] = __riscv_vror_vx_u64m1(state[24], 50, vl);
        B[ 5] = __riscv_vror_vx_u64m1(state[ 3], 36, vl);
        B[ 6] = __riscv_vror_vx_u64m1(state[ 9], 44, vl);
        B[ 7] = __riscv_vror_vx_u64m1(state[10], 61, vl);
        B[ 8] = __riscv_vror_vx_u64m1(state[16], 19, vl);
        B[ 9] = __riscv_vror_vx_u64m1(state[22],  3, vl);
        B[10] = __riscv_vror_vx_u64m1(state[ 1], 63, vl);
        B[11] = __riscv_vror_vx_u64m1(state[ 7], 58, vl);
        B[12] = __riscv_vror_vx_u64m1(state[13], 39, vl);
        B[13] = __riscv_vror_vx_u64m1(state[19], 56, vl);
        B[14] = __riscv_vror_vx_u64m1(state[20], 46, vl);
        B[15] = __riscv_vror_vx_u64m1(state[ 4], 37, vl);
        B[16] = __riscv_vror_vx_u64m1(state[ 5], 28, vl);
        B[17] = __riscv_vror_vx_u64m1(state[11], 54, vl);
        B[18] = __riscv_vror_vx_u64m1(state[17], 49, vl);
        B[19] = __riscv_vror_vx_u64m1(state[23],  8, vl);
        B[20] = __riscv_vror_vx_u64m1(state[ 2],  2, vl);
        B[21] = __riscv_vror_vx_u64m1(state[ 8],  9, vl);
        B[22] = __riscv_vror_vx_u64m1(state[14], 25, vl);
        B[23] = __riscv_vror_vx_u64m1(state[15], 23, vl);
        B[24] = __riscv_vror_vx_u64m1(state[21], 62, vl);

#define CHI_ROW(base) \
        state[(base) + 0] = __riscv_vxor_vv_u64m1(B[(base) + 0], __riscv_vandn_vv_u64m1(B[(base) + 2], B[(base) + 1], vl), vl); \
        state[(base) + 1] = __riscv_vxor_vv_u64m1(B[(base) + 1], __riscv_vandn_vv_u64m1(B[(base) + 3], B[(base) + 2], vl), vl); \
        state[(base) + 2] = __riscv_vxor_vv_u64m1(B[(base) + 2], __riscv_vandn_vv_u64m1(B[(base) + 4], B[(base) + 3], vl), vl); \
        state[(base) + 3] = __riscv_vxor_vv_u64m1(B[(base) + 3], __riscv_vandn_vv_u64m1(B[(base) + 0], B[(base) + 4], vl), vl); \
        state[(base) + 4] = __riscv_vxor_vv_u64m1(B[(base) + 4], __riscv_vandn_vv_u64m1(B[(base) + 1], B[(base) + 0], vl), vl)

        CHI_ROW(0);
        CHI_ROW(5);
        CHI_ROW(10);
        CHI_ROW(15);
        CHI_ROW(20);
#undef CHI_ROW

        state[0] = __riscv_vxor_vx_u64m1(state[0], KeccakF_RoundConstants[round], vl);
    }
}

void shake256_x2_absorb(shake256ctx *ctx0,
                        shake256ctx *ctx1,
                        const uint8_t *in0,
                        const uint8_t *in1,
                        size_t inlen)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t state[25];

    for (size_t i = 0; i < 25; i++) {
        state[i] = __riscv_vmv_v_x_u64m1(0, vl);
    }

    size_t rem = inlen;
    while (rem >= SHAKE256_RATE) {
        for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
            uint64_t pair[2];
            pair[0] = x2_load64_le(in0 + 8 * w);
            pair[1] = x2_load64_le(in1 + 8 * w);
            state[w] = __riscv_vxor_vv_u64m1(state[w], __riscv_vle64_v_u64m1(pair, vl), vl);
        }

        KeccakF1600_StatePermute_x2(state);
        in0 += SHAKE256_RATE;
        in1 += SHAKE256_RATE;
        rem -= SHAKE256_RATE;
    }

    uint8_t tail0[SHAKE256_RATE] = {0};
    uint8_t tail1[SHAKE256_RATE] = {0};
    memcpy(tail0, in0, rem);
    memcpy(tail1, in1, rem);
    tail0[rem] = 0x1F;
    tail1[rem] = 0x1F;
    tail0[SHAKE256_RATE - 1] |= 0x80;
    tail1[SHAKE256_RATE - 1] |= 0x80;

    for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
        uint64_t pair[2];
        pair[0] = x2_load64_le(tail0 + 8 * w);
        pair[1] = x2_load64_le(tail1 + 8 * w);
        state[w] = __riscv_vxor_vv_u64m1(state[w], __riscv_vle64_v_u64m1(pair, vl), vl);
    }

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2];
        __riscv_vse64_v_u64m1(pair, state[i], vl);
        ctx0->ctx[i] = pair[0];
        ctx1->ctx[i] = pair[1];
    }
}

void shake256_x2_squeezeblocks(uint8_t *out0,
                               uint8_t *out1,
                               size_t nblocks,
                               shake256ctx *ctx0,
                               shake256ctx *ctx1)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t state[25];

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2] = {ctx0->ctx[i], ctx1->ctx[i]};
        state[i] = __riscv_vle64_v_u64m1(pair, vl);
    }

    while (nblocks > 0) {
        KeccakF1600_StatePermute_x2(state);
        for (size_t w = 0; w < (SHAKE256_RATE / 8); w++) {
            uint64_t pair[2];
            __riscv_vse64_v_u64m1(pair, state[w], vl);
            x2_store64_le(out0 + 8 * w, pair[0]);
            x2_store64_le(out1 + 8 * w, pair[1]);
        }
        out0 += SHAKE256_RATE;
        out1 += SHAKE256_RATE;
        nblocks--;
    }

    for (size_t i = 0; i < 25; i++) {
        uint64_t pair[2];
        __riscv_vse64_v_u64m1(pair, state[i], vl);
        ctx0->ctx[i] = pair[0];
        ctx1->ctx[i] = pair[1];
    }
}
