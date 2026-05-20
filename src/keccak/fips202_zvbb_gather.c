/* src/keccak/rvv/fips202_rvv_gather.c
 *
 * fips202.h drop-in with a custom LMUL=8 vrgather-based Keccak-f[1600].
 *
 * Design (one-instance, VLEN=256, SEW=64, LMUL=8 → vl=25):
 *   v0           : all 25 Keccak state lanes packed into one register group
 *   Theta        : in-register (vslidedown + vmv.x.s parity extraction,
 *                  scalar D[x] with Zbb rotate, vector diffusion back to v0)
 *   Rho + Pi     : vrgather.vv (Pi permutation) then vror.vv (Rho rotations) [Zvbb]
 *   Chi          : gather-free row-isolated linear slides + explicit wrap patch
 *                  + vandn.vv (~B & C) [Zvbb]
 *   Iota         : vxor.vx on lane 0 with vl=1, then restore vl=25
 *
 * Requires Zvbb (vror.vv, vandn.vv) — compile with -march=..._zbkb_zvbb.
 *
 * Index tables:
 *
 *   pi_src[j]   = source lane index for Pi destination j
 *                 Pi: new[j] = old[pi_src[j]]
 *                 Derived from B[y,(2x+3y)%5] = A[x,y], flat i = x+5y.
 *
 *   ror_amounts[j] = per-lane ROR amount for destination j after Pi
 *                    ror_amounts[j] = (64 - rho_src[pi_src[j]]) % 64
 *                    rho_src[i] = standard Keccak ρ offsets in flat order.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <riscv_vector.h>

#include "fips202.h"

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
 *   group 2 (v16-v23) : vC    — round temporary
 *   group 3 (v24-v31) : free / compiler allocation for extra temporaries
 *
 * The three constant index tables (idx_pi, idx_ror, idx_theta)
 * are
 * loaded once before the round loop and held as C variables.  The compiler
 * keeps them in whichever groups are free or spills them to the vector
 * stack.
 */
static void KeccakF1600_StatePermute(uint64_t *state)
{
    size_t vl = __riscv_vsetvl_e64m8(25);

    vuint64m8_t v0 = __riscv_vle64_v_u64m8(state, vl);

    /* ── Pre-load constant index tables (once per permutation call) ─────── *
     * Eliminates 4 × 24 = 96 vle64.v instructions from the hot round loop. */
    vuint64m8_t idx_pi  = __riscv_vle64_v_u64m8(pi_src,      vl);
    vuint64m8_t idx_ror = __riscv_vle64_v_u64m8(ror_amounts,  vl);
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

        /* ── Chi: row-isolated linear shifts + explicit wrap patch ─────── */
        {
            uint64_t chi_state[25];
            size_t vl5 = __riscv_vsetvl_e64m8(5);

            for (uint64_t row = 0; row < 5; row++) {
                uint64_t base = row * 5;

                vuint64m8_t vRow = __riscv_vslidedown_vx_u64m8(v0, base, vl5);

                uint64_t row0 = __riscv_vmv_x_s_u64m8_u64(vRow);
                vuint64m8_t vTmp = __riscv_vslidedown_vx_u64m8(vRow, 1, vl5);
                uint64_t row1 = __riscv_vmv_x_s_u64m8_u64(vTmp);
                vTmp = __riscv_vslidedown_vx_u64m8(vTmp, 1, vl5);
                uint64_t row2 = __riscv_vmv_x_s_u64m8_u64(vTmp);
                vTmp = __riscv_vslidedown_vx_u64m8(vTmp, 1, vl5);
                uint64_t row3 = __riscv_vmv_x_s_u64m8_u64(vTmp);
                vTmp = __riscv_vslidedown_vx_u64m8(vTmp, 1, vl5);
                uint64_t row4 = __riscv_vmv_x_s_u64m8_u64(vTmp);

                vuint64m8_t vRowP1 = __riscv_vmv_v_x_u64m8(row0, vl5);
                vRowP1 = __riscv_vslide1up_vx_u64m8(vRowP1, row4, vl5);
                vRowP1 = __riscv_vslide1up_vx_u64m8(vRowP1, row3, vl5);
                vRowP1 = __riscv_vslide1up_vx_u64m8(vRowP1, row2, vl5);
                vRowP1 = __riscv_vslide1up_vx_u64m8(vRowP1, row1, vl5);

                vuint64m8_t vRowP2 = __riscv_vmv_v_x_u64m8(row1, vl5);
                vRowP2 = __riscv_vslide1up_vx_u64m8(vRowP2, row0, vl5);
                vRowP2 = __riscv_vslide1up_vx_u64m8(vRowP2, row4, vl5);
                vRowP2 = __riscv_vslide1up_vx_u64m8(vRowP2, row3, vl5);
                vRowP2 = __riscv_vslide1up_vx_u64m8(vRowP2, row2, vl5);

                vuint64m8_t vRowChi = __riscv_vxor_vv_u64m8(
                    vRow,
                    __riscv_vandn_vv_u64m8(vRowP2, vRowP1, vl5),
                    vl5);

                __riscv_vse64_v_u64m8(chi_state + base, vRowChi, vl5);
            }

            vl = __riscv_vsetvl_e64m8(25);
            v0 = __riscv_vle64_v_u64m8(chi_state, vl);
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
    s_inc[s_inc[25] >> 3]  ^= (uint64_t)p   << (8 * (s_inc[25] & 0x07));
    s_inc[(r - 1) >> 3]    ^= (uint64_t)128 << (8 * ((r - 1) & 0x07));
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
