/* experiments/EXP-keccak-rvv-soa-base/src/fips202_rvv_soa.c
 *
 * Base-RVV 1.0 SoA Keccak-f[1600] — NO Zvbb required.
 *
 * Architecture: identical to the promoted Zvbb SoA x2 backend
 * (src/keccak/fips202_zvbb_x2.c) but with:
 *   - vror.vx replaced by vsll.vx + vsrl.vx + vor.vv (3 instr per rotation)
 *   - vandn.vv replaced by vnot.v + vand.vv (2 instr per AND-NOT)
 *
 * Purpose: ablation experiment to measure whether the SoA LMUL=1 architecture
 * alone (without Zvbb-specific instructions) outperforms the scalar Zbb
 * reference on both instruction count (Spike) and cycles (BPI-F3 hardware).
 *
 * The single-stream permutation uses LMUL=8 vl=25 (same as Zvbb version).
 * The x2 paired-stream path uses LMUL=1 vl=2 SoA (same architecture).
 *
 * Target ISA: rv64gcv_zbc_zbb (base Vector 1.0, NO Zvbb/Zvbc)
 * Compile with: -march=rv64gcv_zbb -mrvv-vector-bits=256
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>

#include "fips202.h"

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 0 — Helper macros: base-RVV rotate and AND-NOT
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Vector rotate-right by immediate amount (base-RVV replacement for vror.vx).
 * vror(x, n) = (x >> n) | (x << (64-n))  for n in [0,63]. */
#define VROR_VX_M8(x, n, vl) \
    ((n) == 0 ? (x) : \
     __riscv_vor_vv_u64m8( \
         __riscv_vsrl_vx_u64m8((x), (n), (vl)), \
         __riscv_vsll_vx_u64m8((x), 64 - (n), (vl)), \
         (vl)))

#define VROR_VX_M1(x, n, vl) \
    ((n) == 0 ? (x) : \
     __riscv_vor_vv_u64m1( \
         __riscv_vsrl_vx_u64m1((x), (n), (vl)), \
         __riscv_vsll_vx_u64m1((x), 64 - (n), (vl)), \
         (vl)))

/* Vector AND-NOT: vandn(a, b) = a & ~b  (base-RVV replacement for vandn.vv).
 * Note: Zvbb vandn semantics are vandn(op1, op2) = op1 & ~op2. */
#define VANDN_VV_M8(a, b, vl) \
    __riscv_vand_vv_u64m8((a), __riscv_vnot_v_u64m8((b), (vl)), (vl))

#define VANDN_VV_M1(a, b, vl) \
    __riscv_vand_vv_u64m1((a), __riscv_vnot_v_u64m1((b), (vl)), (vl))

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 1 — LMUL=8 vrgather Keccak-f[1600] (single-stream)
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

/* Pi source indices */
static const uint64_t pi_src[25] = {
     0,  6, 12, 18, 24,
     3,  9, 10, 16, 22,
     1,  7, 13, 19, 20,
     4,  5, 11, 17, 23,
     2,  8, 14, 15, 21
};

/* ROR amounts for Rho (applied after Pi gather) */
static const uint64_t ror_amounts[25] = {
     0, 20, 21, 43, 50,
    36, 44, 61, 19,  3,
    63, 58, 39, 56, 46,
    37, 28, 54, 49,  8,
     2,  9, 25, 23, 62
};

/* Chi x+1 and x+2 indices */
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

/* Theta column-broadcast index */
static const uint64_t theta_col_idx[25] = {
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4,
    0, 1, 2, 3, 4
};

static inline uint64_t rol1_zbb(uint64_t x)
{
    return ROL(x, 1);
}


/*
 * KeccakF1600_StatePermute — LMUL=8 vrgather, base-RVV (no Zvbb).
 *
 * Same architecture as fips202_zvbb_x2.c Section 1, but:
 *   - vror.vv/vror.vx → VROR_VX_M8 macro (vsll+vsrl+vor)
 *   - vandn.vv → VANDN_VV_M8 macro (vnot+vand)
 */
static void KeccakF1600_StatePermute(uint64_t *state)
{
    size_t vl = __riscv_vsetvl_e64m8(25);

    vuint64m8_t v0 = __riscv_vle64_v_u64m8(state, vl);

    /* Pre-load constant index tables */
    vuint64m8_t idx_pi    = __riscv_vle64_v_u64m8(pi_src,        vl);
    vuint64m8_t idx_ror   = __riscv_vle64_v_u64m8(ror_amounts,   vl);
    vuint64m8_t idx_x1    = __riscv_vle64_v_u64m8(chi_x1,        vl);
    vuint64m8_t idx_x2    = __riscv_vle64_v_u64m8(chi_x2,        vl);
    vuint64m8_t idx_theta = __riscv_vle64_v_u64m8(theta_col_idx, vl);

    for (int round = 0; round < NROUNDS; round++) {

        /* ── Theta ──────────────────────────────────────────────────────── */
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

        /* ── Rho + Pi: vrgather (Pi) then vector rotate (Rho) ───────────── */
        {
            vuint64m8_t t = __riscv_vrgather_vv_u64m8(v0, idx_pi, vl);
            /* Base-RVV rotate: vsrl by idx_ror, vsll by (64-idx_ror), OR.
             * We use vrgatherei16 for the variable-amount rotate since we need
             * per-element different rotation amounts from idx_ror. */
            vuint64m8_t complement = __riscv_vrsub_vx_u64m8(idx_ror, 64, vl);
            vuint64m8_t lo = __riscv_vsrl_vv_u64m8(t, idx_ror, vl);
            vuint64m8_t hi = __riscv_vsll_vv_u64m8(t, complement, vl);
            v0 = __riscv_vor_vv_u64m8(lo, hi, vl);
        }

        /* ── Chi: vrgather + base-RVV AND-NOT ───────────────────────────── */
        {
            vuint64m8_t vB = __riscv_vrgather_vv_u64m8(v0, idx_x1, vl);
            vuint64m8_t vC = __riscv_vrgather_vv_u64m8(v0, idx_x2, vl);
            /* vandn(vC, vB) = vC & ~vB → ~vB & vC = (~A[x+1]) & A[x+2] */
            v0 = __riscv_vxor_vv_u64m8(v0, VANDN_VV_M8(vC, vB, vl), vl);
        }

        /* ── Iota ───────────────────────────────────────────────────────── */
        {
            size_t vl1 = __riscv_vsetvl_e64m8(1);
            v0 = __riscv_vxor_vx_u64m8(v0, KeccakF_RoundConstants[round], vl1);
            vl = __riscv_vsetvl_e64m8(25);
        }
    }

    __riscv_vse64_v_u64m8(state, v0, vl);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 2 — Helper: load/store 64-bit words in little-endian order
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint64_t load64_le(const uint8_t *x)
{
    uint64_t r;
    __builtin_memcpy(&r, x, sizeof(r));
    return r;
}

static inline void store64_le(uint8_t *x, uint64_t u)
{
    __builtin_memcpy(x, &u, sizeof(u));
}

/* XOR len bytes into the Keccak state at byte offset pos.
 * Uses RVV 1.0 VLA byte-streaming (same as promoted Zvbb version). */
static inline void keccak_xor_bytes(uint64_t *s_inc,
                                    size_t pos,
                                    const uint8_t *in,
                                    size_t len)
{
    size_t i = 0;

    /* Scalar pre-alignment */
    while (i < len && ((pos + i) & 0x07)) {
        size_t p = pos + i;
        s_inc[p >> 3] ^= (uint64_t)in[i] << (8 * (p & 0x07));
        i++;
    }

    /* RVV VLA byte-streaming */
    {
        uint8_t       *dst       = (uint8_t *)s_inc + (pos + i);
        const uint8_t *src       = in + i;
        size_t         remaining = len - i;

        while (remaining > 0) {
            size_t vl = __riscv_vsetvl_e8m8(remaining);
            vuint8m8_t vdst = __riscv_vle8_v_u8m8(dst, vl);
            vuint8m8_t vsrc = __riscv_vle8_v_u8m8(src, vl);
            __riscv_vse8_v_u8m8(dst, __riscv_vxor_vv_u8m8(vdst, vsrc, vl), vl);
            dst       += vl;
            src       += vl;
            remaining -= vl;
        }
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
 * Section 3 — Absorb / squeeze helpers (identical to Zvbb version)
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

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 5 — x2 SoA paired-stream API (base-RVV, no Zvbb)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Fixed-length RVV type for arrays of vector values (Clang 21+). */
typedef vuint64m1_t vu64m1_fixed_t __attribute__((riscv_rvv_vector_bits(HQC_VECTOR_BITS)));

static inline size_t keccak_x2_setvl(void)
{
    return __riscv_vsetvl_e64m1(2);
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

/* SoA permutation core: base-RVV (no Zvbb), LMUL=1, vl=2 */
static void KeccakF1600_StatePermute_x2(vu64m1_fixed_t *state)
{
    const size_t vl = keccak_x2_setvl();
    vu64m1_fixed_t C[5];
    vu64m1_fixed_t D[5];
    vu64m1_fixed_t B[25];

    for (int round = 0; round < NROUNDS; round++) {
        /* ── Theta: column parity ───────────────────────────────────────── */
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

        /* D[x] = C[x-1] ^ ROL(C[x+1], 1)  — ROL(x,1) = ROR(x,63) */
        D[0] = __riscv_vxor_vv_u64m1(C[4], VROR_VX_M1(C[1], 63, vl), vl);
        D[1] = __riscv_vxor_vv_u64m1(C[0], VROR_VX_M1(C[2], 63, vl), vl);
        D[2] = __riscv_vxor_vv_u64m1(C[1], VROR_VX_M1(C[3], 63, vl), vl);
        D[3] = __riscv_vxor_vv_u64m1(C[2], VROR_VX_M1(C[4], 63, vl), vl);
        D[4] = __riscv_vxor_vv_u64m1(C[3], VROR_VX_M1(C[0], 63, vl), vl);

        /* Apply D[x] across all 5 rows */
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

        /* ── Rho + Pi: compile-time resolved, base-RVV rotate ────────────── */
        B[ 0] = state[ 0];  /* ror=0, identity */
        B[ 1] = VROR_VX_M1(state[ 6], 20, vl);
        B[ 2] = VROR_VX_M1(state[12], 21, vl);
        B[ 3] = VROR_VX_M1(state[18], 43, vl);
        B[ 4] = VROR_VX_M1(state[24], 50, vl);
        B[ 5] = VROR_VX_M1(state[ 3], 36, vl);
        B[ 6] = VROR_VX_M1(state[ 9], 44, vl);
        B[ 7] = VROR_VX_M1(state[10], 61, vl);
        B[ 8] = VROR_VX_M1(state[16], 19, vl);
        B[ 9] = VROR_VX_M1(state[22],  3, vl);
        B[10] = VROR_VX_M1(state[ 1], 63, vl);
        B[11] = VROR_VX_M1(state[ 7], 58, vl);
        B[12] = VROR_VX_M1(state[13], 39, vl);
        B[13] = VROR_VX_M1(state[19], 56, vl);
        B[14] = VROR_VX_M1(state[20], 46, vl);
        B[15] = VROR_VX_M1(state[ 4], 37, vl);
        B[16] = VROR_VX_M1(state[ 5], 28, vl);
        B[17] = VROR_VX_M1(state[11], 54, vl);
        B[18] = VROR_VX_M1(state[17], 49, vl);
        B[19] = VROR_VX_M1(state[23],  8, vl);
        B[20] = VROR_VX_M1(state[ 2],  2, vl);
        B[21] = VROR_VX_M1(state[ 8],  9, vl);
        B[22] = VROR_VX_M1(state[14], 25, vl);
        B[23] = VROR_VX_M1(state[15], 23, vl);
        B[24] = VROR_VX_M1(state[21], 62, vl);

        /* ── Chi: base-RVV AND-NOT ──────────────────────────────────────── */
#define CHI_ROW(base) \
        state[(base) + 0] = __riscv_vxor_vv_u64m1(B[(base) + 0], VANDN_VV_M1(B[(base) + 2], B[(base) + 1], vl), vl); \
        state[(base) + 1] = __riscv_vxor_vv_u64m1(B[(base) + 1], VANDN_VV_M1(B[(base) + 3], B[(base) + 2], vl), vl); \
        state[(base) + 2] = __riscv_vxor_vv_u64m1(B[(base) + 2], VANDN_VV_M1(B[(base) + 4], B[(base) + 3], vl), vl); \
        state[(base) + 3] = __riscv_vxor_vv_u64m1(B[(base) + 3], VANDN_VV_M1(B[(base) + 0], B[(base) + 4], vl), vl); \
        state[(base) + 4] = __riscv_vxor_vv_u64m1(B[(base) + 4], VANDN_VV_M1(B[(base) + 1], B[(base) + 0], vl), vl)

        CHI_ROW(0);
        CHI_ROW(5);
        CHI_ROW(10);
        CHI_ROW(15);
        CHI_ROW(20);
#undef CHI_ROW

        /* ── Iota ───────────────────────────────────────────────────────── */
        state[0] = __riscv_vxor_vx_u64m1(state[0], KeccakF_RoundConstants[round], vl);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Section 6 — x2 Public API wrappers
 * ═══════════════════════════════════════════════════════════════════════════ */

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
