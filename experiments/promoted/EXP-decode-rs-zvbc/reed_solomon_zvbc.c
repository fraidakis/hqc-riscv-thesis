/* experiments/EXP-decode-rs-zvbc/reed_solomon_zvbc.c
 *
 * Phase 3b+ — Zvbc lane-packed GF(2^8) vectorisation for Reed-Solomon
 * Berlekamp-Massey (Axis B) and Forney (Axis D) phases.
 *
 * Status:
 *   Axis A — compute_syndromes:   DONE (inherited from reed_solomon_rvv.c)
 *   Axis B — compute_elp:         DONE — three inner loops vectorised via Zvbc
 *   Axis C — compute_roots / fft: scalar fallback (unchanged)
 *   Axis D — compute_error_values:DONE — inv-power dot-product + inner-k batch
 *
 * Vectorisation strategy (common to all axes):
 *
 *   GF(2^8) multiply via Zvbc vclmul.vv (SEW=64):
 *     - One GF element (8 bits) in bits [7:0] of a 64-bit lane, upper 56 bits zero.
 *     - vclmul.vv gives the 14-bit carry-less product in bits [14:0].
 *     - Two-pass inline Barrett reduction (vsrl/vand/vxor) folds back modulo
 *       PARAM_GF_POLY = 0x11D (x^8+x^4+x^3+x^2+1).
 *
 *   LMUL=4, SEW=64: vl=16 at VLEN=256; VLA — correct at any VLEN≥64.
 *     DELTA=15 (HQC-128): 1 pass per inner loop.
 *     DELTA=16 (HQC-192): 1 pass per inner loop.
 *     DELTA=29 (HQC-256): 2 passes per inner loop.
 *
 * Axis B — compute_elp inner loops:
 *   1. Sigma SAXPY:      sigma[1..n] ^= dd * X_sigma_p[1..n]
 *   2. X_sigma_p blend:  X_sigma_p[1..D] = mask12 ? sigma_copy[0..D-1]
 *                                                  : X_sigma_p[0..D-1]
 *      (u16m4 integer SIMD, no GF multiply)
 *   3. Discrepancy dot:  d ^= sum_i gf_mul(sigma[i], syndromes[mu+1-i])
 *      (vlse16 negative-stride load + vclmul + vredxor)
 *
 * Axis D — compute_error_values inner loops:
 *   1. Inv-power precompute:  scalar chain inv_pow[j] = inverse^j (unavoidable)
 *   2. Forney dot-product:    tmp1 = XOR_j inv_pow[j] * z[j]  (vectorised)
 *   3. Inner-k batch:         inner_k[k] = 1 ^ gf_mul(inverse, beta_j[(i+k)%D])
 *      computed in two contiguous segments (A: i+1..D-1, B: 0..i-1) via vclmul;
 *      final sequential product over inner_k remains scalar.
 *
 * All other sub-functions (roots, z-poly, correct_errors, encode) are copied
 * verbatim from vendor/hqc-ref/src/ref/reed_solomon.c.
 *
 * Interface (unchanged):
 *   void reed_solomon_decode(uint64_t *msg, uint64_t *cdw);
 *   void reed_solomon_encode(uint64_t *cdw, const uint64_t *msg);
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>

#include "reed_solomon.h"   /* alpha_ij_pow (level-specific) */
#include "gf.h"             /* gf_mul, gf_inverse, gf_exp, gf_log */
#include "fft.h"            /* fft, fft_retrieve_error_poly */
#include "parameters.h"     /* PARAM_DELTA, PARAM_N1, PARAM_M, PARAM_FFT, PARAM_K, PARAM_G */
#include "crypto_memset.h"  /* memset_zero */

/* ── GF(2^8) inline Barrett reduction ───────────────────────────────────────
 *
 * Reduces a vector of 14-bit carry-less products (each in bits [14:0] of a
 * 64-bit lane) modulo PARAM_GF_POLY = 0x11D (x^8+x^4+x^3+x^2+1).
 *
 * x^8 ≡ x^4 + x^3 + x^2 + 1  mod poly.
 * Two passes suffice for degree ≤ 14:
 *   Pass 1: extract bits [14:8], fold into [7:0] via shifts 0,2,3,4.
 *            Result: degree ≤ 10.
 *   Pass 2: extract bits [10:8], fold again. Result: degree < 8. ✓
 *
 * Constant-time: no data-dependent branches.
 */
static inline vuint64m4_t gf_reduce_vec_u64m4(vuint64m4_t v, size_t vl)
{
    vuint64m4_t high;

    /* Pass 1 — fold bits [14:8] */
    high = __riscv_vsrl_vx_u64m4(v, 8, vl);
    v    = __riscv_vand_vx_u64m4(v, 0xFFu, vl);
    v    = __riscv_vxor_vv_u64m4(v, high, vl);
    v    = __riscv_vxor_vv_u64m4(v, __riscv_vsll_vx_u64m4(high, 2, vl), vl);
    v    = __riscv_vxor_vv_u64m4(v, __riscv_vsll_vx_u64m4(high, 3, vl), vl);
    v    = __riscv_vxor_vv_u64m4(v, __riscv_vsll_vx_u64m4(high, 4, vl), vl);

    /* Pass 2 — fold bits [10:8] left by pass 1 */
    high = __riscv_vsrl_vx_u64m4(v, 8, vl);
    v    = __riscv_vand_vx_u64m4(v, 0xFFu, vl);
    v    = __riscv_vxor_vv_u64m4(v, high, vl);
    v    = __riscv_vxor_vv_u64m4(v, __riscv_vsll_vx_u64m4(high, 2, vl), vl);
    v    = __riscv_vxor_vv_u64m4(v, __riscv_vsll_vx_u64m4(high, 3, vl), vl);
    v    = __riscv_vxor_vv_u64m4(v, __riscv_vsll_vx_u64m4(high, 4, vl), vl);

    return v;
}

/* ── Axis A: Vectorised syndrome computation (inherited) ─────────────────────
 *
 * Identical to reed_solomon_rvv.c — included here so this file is a complete
 * drop-in replacement.
 */
static void compute_syndromes_rvv(uint16_t *syndromes, const uint8_t *cdw)
{
    for (size_t i = 0; i < 2u * PARAM_DELTA; i++) {
        uint16_t acc = (uint16_t)cdw[0];

        const uint8_t  *pa  = cdw + 1;
        const uint16_t *pb  = alpha_ij_pow[i];
        size_t          rem = PARAM_N1 - 1u;

        while (rem > 0) {
            size_t vl = __riscv_vsetvl_e64m4(rem);

            vuint8mf2_t  a8  = __riscv_vle8_v_u8mf2(pa, vl);
            vuint64m4_t  a64 = __riscv_vzext_vf8_u64m4(a8, vl);

            vuint16m1_t  b16 = __riscv_vle16_v_u16m1(pb, vl);
            vuint64m4_t  b64 = __riscv_vzext_vf4_u64m4(b16, vl);

            vuint64m4_t prod = __riscv_vclmul_vv_u64m4(a64, b64, vl);
            prod = gf_reduce_vec_u64m4(prod, vl);

            vuint64m1_t zero_v = __riscv_vmv_v_x_u64m1(0u, 1);
            vuint64m1_t red    = __riscv_vredxor_vs_u64m4_u64m1(prod, zero_v, vl);
            acc ^= (uint16_t)__riscv_vmv_x_s_u64m1_u64(red);

            pa  += vl;
            pb  += vl;
            rem -= vl;
        }

        syndromes[i] = acc;
    }
}

/* ── Axis B: Vectorised Berlekamp-Massey ELP ─────────────────────────────────
 *
 * Runs the standard BM algorithm but replaces three inner loops with Zvbc
 * vector operations:
 *
 *  Loop 1 — sigma SAXPY:
 *    sigma[1..n] ^= dd * X_sigma_p[1..n]
 *    (vmv_v_x broadcast + vclmul.vv + inline reduce + vnsrl narrow-store)
 *
 *  Loop 2 — X_sigma_p blend:
 *    X_sigma_p[1..D] = mask12 ? sigma_copy[0..D-1] : X_sigma_p[0..D-1]
 *    (u16m4 integer SIMD — no GF multiply needed, avoids scalar loop)
 *
 *  Loop 3 — discrepancy dot-product:
 *    d = s[mu+1] XOR sum_{i=1}^{n} gf_mul(sigma[i], s[mu+1-i])
 *    (vlse16 negative-stride load + vclmul.vv + vredxor)
 *
 * All scalar control flow (masks, pp, d_p, degree tracking) is unchanged.
 */
static uint16_t compute_elp_zvbc(uint16_t *sigma, const uint16_t *syndromes)
{
    uint16_t deg_sigma      = 0;
    uint16_t deg_sigma_p    = 0;
    uint16_t deg_sigma_copy = 0;
    uint16_t sigma_copy[PARAM_DELTA + 1]  = {0};
    uint16_t X_sigma_p[PARAM_DELTA + 1]   = {0, 1};
    uint16_t pp   = (uint16_t)-1;
    uint16_t d_p  = 1;
    uint16_t d    = syndromes[0];
    uint16_t mask1, mask2, mask12;
    uint16_t deg_X, deg_X_sigma_p, dd, mu;

    sigma[0] = 1;
    for (mu = 0; mu < 2u * PARAM_DELTA; ++mu) {
        memcpy(sigma_copy, sigma, 2u * PARAM_DELTA);
        deg_sigma_copy = deg_sigma;

        dd = gf_mul(d, gf_inverse(d_p));

        /* ── Loop 1: sigma SAXPY ─────────────────────────────────────────── *
         * sigma[1..n] ^= dd * X_sigma_p[1..n]                               *
         * n = min(mu+1, PARAM_DELTA)                                         */
        {
            uint16_t n_elts = (mu + 1u < (uint16_t)PARAM_DELTA)
                              ? (uint16_t)(mu + 1u)
                              : (uint16_t)PARAM_DELTA;
            const uint16_t *pxsp = X_sigma_p + 1;
            uint16_t       *psig = sigma + 1;
            size_t          rem  = (size_t)n_elts;

            while (rem > 0) {
                size_t vl = __riscv_vsetvl_e64m4(rem);

                /* load X_sigma_p[1..n], zero-extend to 64-bit lanes */
                vuint16m1_t xsp16 = __riscv_vle16_v_u16m1(pxsp, vl);
                vuint64m4_t xsp64 = __riscv_vzext_vf4_u64m4(xsp16, vl);

                /* broadcast scalar dd to all lanes */
                vuint64m4_t dd64  = __riscv_vmv_v_x_u64m4((uint64_t)(dd & 0xFFu), vl);

                /* carry-less multiply, inline GF(2^8) reduction */
                vuint64m4_t prod  = __riscv_vclmul_vv_u64m4(dd64, xsp64, vl);
                prod = gf_reduce_vec_u64m4(prod, vl);

                /* load sigma, XOR with product */
                vuint16m1_t sig16 = __riscv_vle16_v_u16m1(psig, vl);
                vuint64m4_t sig64 = __riscv_vzext_vf4_u64m4(sig16, vl);
                vuint64m4_t res64 = __riscv_vxor_vv_u64m4(sig64, prod, vl);

                /* narrow u64m4 → u32m2 → u16m1 (values fit in 8 bits) */
                vuint32m2_t res32 = __riscv_vnsrl_wx_u32m2(res64, 0, vl);
                vuint16m1_t res16 = __riscv_vnsrl_wx_u16m1(res32, 0, vl);
                __riscv_vse16_v_u16m1(psig, res16, vl);

                pxsp += vl;  psig += vl;  rem -= vl;
            }
        }

        deg_X         = mu - pp;
        deg_X_sigma_p = deg_X + deg_sigma_p;

        mask1 = -((uint16_t)-d >> 15);
        mask2 = -((uint16_t)(deg_sigma - deg_X_sigma_p) >> 15);

        volatile uint16_t mask12__ = mask1 & mask2;
        mask12 = mask12__;
        deg_sigma ^= mask12 & (deg_X_sigma_p ^ deg_sigma);

        if (mu == 2u * PARAM_DELTA - 1u)
            break;

        pp  ^= mask12 & (mu ^ pp);
        d_p ^= mask12 & (d  ^ d_p);

        /* ── Loop 2: X_sigma_p blend ─────────────────────────────────────── *
         * X_sigma_p[1..DELTA] = mask12 ? sigma_copy[0..DELTA-1]             *
         *                              : X_sigma_p[0..DELTA-1]              *
         * blend = (mask12 & sc) ^ (~mask12 & xsp)  (integer SIMD, u16m4)   */
        {
            uint16_t       *pout = X_sigma_p + 1;
            const uint16_t *psc  = sigma_copy;
            const uint16_t *pxsp = X_sigma_p;
            size_t          rem  = PARAM_DELTA;

            while (rem > 0) {
                size_t vl = __riscv_vsetvl_e16m4(rem);
                vuint16m4_t sc16   = __riscv_vle16_v_u16m4(psc, vl);
                vuint16m4_t xsp16  = __riscv_vle16_v_u16m4(pxsp, vl);
                vuint16m4_t vmask  = __riscv_vmv_v_x_u16m4(mask12, vl);
                vuint16m4_t nvmask = __riscv_vmv_v_x_u16m4((uint16_t)~mask12, vl);
                vuint16m4_t res    = __riscv_vxor_vv_u16m4(
                                         __riscv_vand_vv_u16m4(vmask,  sc16,  vl),
                                         __riscv_vand_vv_u16m4(nvmask, xsp16, vl),
                                         vl);
                __riscv_vse16_v_u16m4(pout, res, vl);
                psc += vl;  pxsp += vl;  pout += vl;  rem -= vl;
            }
        }

        deg_sigma_p ^= mask12 & (deg_sigma_copy ^ deg_sigma_p);

        /* ── Loop 3: discrepancy dot-product ─────────────────────────────── *
         * d = s[mu+1] XOR sum_{i=1}^{n} gf_mul(sigma[i], s[mu+1-i])        *
         * n = min(mu+1, PARAM_DELTA)                                         *
         * Syndromes are accessed in reverse via vlse16 negative-stride load. */
        {
            uint16_t n_elts = (mu + 1u < (uint16_t)PARAM_DELTA)
                              ? (uint16_t)(mu + 1u)
                              : (uint16_t)PARAM_DELTA;
            const uint16_t *psig  = sigma + 1;
            const uint16_t *psyn  = syndromes + mu;   /* start: syndromes[mu] */
            d = syndromes[mu + 1u];
            size_t rem = (size_t)n_elts;

            while (rem > 0) {
                size_t vl = __riscv_vsetvl_e64m4(rem);

                /* sigma[1..n] */
                vuint16m1_t sig16 = __riscv_vle16_v_u16m1(psig, vl);
                vuint64m4_t sig64 = __riscv_vzext_vf4_u64m4(sig16, vl);

                /* syndromes[mu..mu+1-n] with stride -2 bytes (reverse) */
                vuint16m1_t syn16 = __riscv_vlse16_v_u16m1(
                                        psyn,
                                        -(ptrdiff_t)sizeof(uint16_t),
                                        vl);
                vuint64m4_t syn64 = __riscv_vzext_vf4_u64m4(syn16, vl);

                vuint64m4_t prod  = __riscv_vclmul_vv_u64m4(sig64, syn64, vl);
                prod = gf_reduce_vec_u64m4(prod, vl);

                vuint64m1_t zero_v = __riscv_vmv_v_x_u64m1(0u, 1);
                vuint64m1_t red    = __riscv_vredxor_vs_u64m4_u64m1(prod, zero_v, vl);
                d ^= (uint16_t)__riscv_vmv_x_s_u64m1_u64(red);

                psig += vl;  psyn -= vl;  rem -= vl;
            }
        }
    }
    return deg_sigma;
}

/* ── Axis C: Root-finding (scalar fallback) ──────────────────────────────── */
static void compute_roots(uint8_t *error, uint16_t *sigma)
{
    uint16_t w[1 << PARAM_M] = {0};
    fft(w, sigma, PARAM_DELTA + 1);
    fft_retrieve_error_poly(error, w);
}

/* ── Axis C helper: z-polynomial (scalar) ───────────────────────────────── */
static void compute_z_poly(uint16_t *z, const uint16_t *sigma,
                           const uint16_t degree, const uint16_t *syndromes)
{
    size_t   i, j;
    uint16_t mask;

    z[0] = 1;
    for (i = 1; i < PARAM_DELTA + 1u; ++i) {
        mask = -((uint16_t)(i - degree - 1u) >> 15);
        z[i] = mask & sigma[i];
    }
    z[1] ^= syndromes[0];
    for (i = 2; i <= PARAM_DELTA; ++i) {
        mask = -((uint16_t)(i - degree - 1u) >> 15);
        z[i] ^= mask & syndromes[i - 1];
        for (j = 1; j < i; ++j)
            z[i] ^= mask & gf_mul(sigma[j], syndromes[i - j - 1]);
    }
}

/* ── Axis D: Vectorised Forney error-value computation ───────────────────────
 *
 * Vectorises two structures inside the per-error-location loop:
 *
 *  Step 2 — Forney numerator dot-product:
 *    tmp1 = XOR_{j=1}^{DELTA} (inverse^j * z[j])
 *    Precompute inv_pow[j]=inverse^j scalar (sequential chain, unavoidable),
 *    then vectorised dot-product over inv_pow[1..D] vs z[1..D].
 *
 *  Step 3 — Inner-k inverse-beta batch:
 *    inner_k[k] = 1 ^ gf_mul(inverse, beta_j[(i+k)%DELTA])  for k=0..D-2
 *    Split into two contiguous segments to avoid modulo-index gather:
 *      Seg A: beta_j[i+1..D-1]   (length D-1-i)
 *      Seg B: beta_j[0..i-1]     (length i)
 *    Both segments vectorised via vclmul.vv + inline reduction.
 *    The final sequential product over inner_k remains scalar (O(DELTA)).
 *
 * All scalar control (mask1, mask2, delta_counter, delta_real_value) is
 * unchanged for constant-time correctness.
 */
static void compute_error_values_zvbc(uint16_t *error_values,
                                      const uint16_t *z, const uint8_t *error)
{
    uint16_t beta_j[PARAM_DELTA]  = {0};
    uint16_t e_j[PARAM_DELTA]     = {0};
    uint16_t delta_counter, delta_real_value, found;
    uint16_t mask1, mask2, tmp1, tmp2, inverse;

    /* ── Locate error positions and collect beta_j (scalar, unchanged) ───── */
    delta_counter = 0;
    for (size_t i = 0; i < PARAM_N1; i++) {
        found = 0;
        mask1 = (uint16_t)(-((int32_t)error[i]) >> 31);
        for (size_t j = 0; j < PARAM_DELTA; j++) {
            mask2 = ~((uint16_t)(-((int32_t)j ^ delta_counter) >> 31));
            beta_j[j] += mask1 & mask2 & gf_exp[i];
            found      += mask1 & mask2 & 1u;
        }
        delta_counter += found;
    }
    delta_real_value = delta_counter;

    /* ── Per-error-location Forney evaluation ──────────────────────────── */
    /* Scratch buffers on stack: PARAM_DELTA ≤ 29 → ≤ 60 bytes each.       */
    uint16_t inv_pow[PARAM_DELTA + 1];    /* inv_pow[j] = inverse^j          */
    uint16_t inner_k[PARAM_DELTA];        /* inner_k[m] = 1^gf_mul(inv,β_m) */

    for (size_t i = 0; i < PARAM_DELTA; ++i) {
        inverse = gf_inverse(beta_j[i]);

        /* ── Step 1: scalar precompute inv_pow[1..DELTA] ─────────────────── *
         * inverse^j for j=0..DELTA.  Sequential chain — cannot be           *
         * parallelised.  Only DELTA multiplications total.                   */
        inv_pow[0] = 1;
        for (size_t j = 1; j <= PARAM_DELTA; ++j)
            inv_pow[j] = gf_mul(inv_pow[j - 1u], inverse);

        /* ── Step 2: vectorised Forney dot-product ───────────────────────── *
         * tmp1 = XOR_{j=1}^{DELTA} inv_pow[j] * z[j]                       */
        tmp1 = 0;
        {
            const uint16_t *pi  = inv_pow + 1;
            const uint16_t *pz  = z + 1;
            size_t          rem = PARAM_DELTA;

            while (rem > 0) {
                size_t vl = __riscv_vsetvl_e64m4(rem);

                vuint16m1_t vi16  = __riscv_vle16_v_u16m1(pi, vl);
                vuint64m4_t vi64  = __riscv_vzext_vf4_u64m4(vi16, vl);
                vuint16m1_t vz16  = __riscv_vle16_v_u16m1(pz, vl);
                vuint64m4_t vz64  = __riscv_vzext_vf4_u64m4(vz16, vl);

                vuint64m4_t prod  = __riscv_vclmul_vv_u64m4(vi64, vz64, vl);
                prod = gf_reduce_vec_u64m4(prod, vl);

                vuint64m1_t zero_v = __riscv_vmv_v_x_u64m1(0u, 1);
                vuint64m1_t red    = __riscv_vredxor_vs_u64m4_u64m1(prod, zero_v, vl);
                tmp1 ^= (uint16_t)__riscv_vmv_x_s_u64m1_u64(red);

                pi += vl;  pz += vl;  rem -= vl;
            }
        }

        /* ── Step 3: vectorised inner-k batch then scalar product ─────────── *
         * Original: for k=1..D-1: tmp2 *= 1 ^ gf_mul(inv, beta[(i+k)%D])   *
         *                                                                     *
         * Decompose beta_j[(i+k)%D] for k=1..D-1 into two contiguous segs: *
         *   Seg A: beta_j[i+1 .. D-1]   length = D-1-i                      *
         *   Seg B: beta_j[0  .. i-1]    length = i                          *
         * (Total = D-1 elements, matches k=1..D-1.)                         *
         *                                                                     *
         * Each segment: vclmul(inverse, beta) + inline reduce + (1^result)  *
         * stored into inner_k[0..D-2].  Sequential product follows scalar.  */
        {
            size_t len_A = (size_t)(PARAM_DELTA - 1u) - i;
            size_t len_B = i;
            uint16_t *out = inner_k;

            /* Segment A: beta_j[i+1 .. PARAM_DELTA-1] */
            if (len_A > 0) {
                const uint16_t *pb = beta_j + i + 1;
                size_t          rem = len_A;
                while (rem > 0) {
                    size_t vl = __riscv_vsetvl_e64m4(rem);
                    vuint64m4_t vinv  = __riscv_vmv_v_x_u64m4(
                                           (uint64_t)(inverse & 0xFFu), vl);
                    vuint16m1_t b16   = __riscv_vle16_v_u16m1(pb, vl);
                    vuint64m4_t b64   = __riscv_vzext_vf4_u64m4(b16, vl);
                    vuint64m4_t prod  = __riscv_vclmul_vv_u64m4(vinv, b64, vl);
                    prod = gf_reduce_vec_u64m4(prod, vl);
                    /* XOR product with 1 → (1 ^ gf_mul(inv, beta)) */
                    vuint64m4_t one64 = __riscv_vmv_v_x_u64m4(1u, vl);
                    vuint64m4_t res64 = __riscv_vxor_vv_u64m4(prod, one64, vl);
                    vuint32m2_t res32 = __riscv_vnsrl_wx_u32m2(res64, 0, vl);
                    vuint16m1_t res16 = __riscv_vnsrl_wx_u16m1(res32, 0, vl);
                    __riscv_vse16_v_u16m1(out, res16, vl);
                    pb += vl;  out += vl;  rem -= vl;
                }
            }

            /* Segment B: beta_j[0 .. i-1] */
            if (len_B > 0) {
                const uint16_t *pb  = beta_j;
                size_t          rem = len_B;
                while (rem > 0) {
                    size_t vl = __riscv_vsetvl_e64m4(rem);
                    vuint64m4_t vinv  = __riscv_vmv_v_x_u64m4(
                                           (uint64_t)(inverse & 0xFFu), vl);
                    vuint16m1_t b16   = __riscv_vle16_v_u16m1(pb, vl);
                    vuint64m4_t b64   = __riscv_vzext_vf4_u64m4(b16, vl);
                    vuint64m4_t prod  = __riscv_vclmul_vv_u64m4(vinv, b64, vl);
                    prod = gf_reduce_vec_u64m4(prod, vl);
                    vuint64m4_t one64 = __riscv_vmv_v_x_u64m4(1u, vl);
                    vuint64m4_t res64 = __riscv_vxor_vv_u64m4(prod, one64, vl);
                    vuint32m2_t res32 = __riscv_vnsrl_wx_u32m2(res64, 0, vl);
                    vuint16m1_t res16 = __riscv_vnsrl_wx_u16m1(res32, 0, vl);
                    __riscv_vse16_v_u16m1(out, res16, vl);
                    pb += vl;  out += vl;  rem -= vl;
                }
            }

            /* Sequential product over inner_k[0..DELTA-2] */
            tmp2 = 1;
            for (size_t k = 0; k < (size_t)(PARAM_DELTA - 1u); ++k)
                tmp2 = gf_mul(tmp2, inner_k[k]);
        }

        mask1  = (uint16_t)(((int16_t)i - (int16_t)delta_real_value) >> 15);
        e_j[i] = mask1 & gf_mul(tmp1, gf_inverse(tmp2));
    }

    /* ── Scatter error values (scalar, unchanged) ────────────────────────── */
    delta_counter = 0;
    for (size_t i = 0; i < PARAM_N1; ++i) {
        found = 0;
        mask1 = (uint16_t)(-((int32_t)error[i]) >> 31);
        for (size_t j = 0; j < PARAM_DELTA; j++) {
            mask2 = ~((uint16_t)(-((int32_t)j ^ delta_counter) >> 31));
            error_values[i] += mask1 & mask2 & e_j[j];
            found            += mask1 & mask2 & 1u;
        }
        delta_counter += found;
    }
}

/* ── XOR error correction (scalar, unchanged) ─────────────────────────────── */
static void correct_errors(uint8_t *cdw, const uint16_t *error_values)
{
    for (size_t i = 0; i < PARAM_N1; ++i)
        cdw[i] ^= (uint8_t)error_values[i];
}

/* ── Public entry point ──────────────────────────────────────────────────────
 *
 * Drop-in replacement for reed_solomon_decode().
 * Axes A, B, D vectorised; Axis C (root finding) remains scalar.
 */
void reed_solomon_decode(uint64_t *msg, uint64_t *cdw)
{
    uint8_t  cdw_bytes[PARAM_N1]         = {0};
    uint16_t syndromes[2 * PARAM_DELTA]  = {0};
    uint16_t sigma[1 << PARAM_FFT]       = {0};
    uint8_t  error[1 << PARAM_M]         = {0};
    uint16_t z[PARAM_N1]                 = {0};
    uint16_t error_values[PARAM_N1]      = {0};
    uint16_t deg;

    memcpy(cdw_bytes, cdw, PARAM_N1);

    /* Axis A — vectorised syndrome computation */
    compute_syndromes_rvv(syndromes, cdw_bytes);

    /* Axis B — vectorised Berlekamp-Massey ELP */
    deg = compute_elp_zvbc(sigma, syndromes);

    /* Axis C — scalar root finding via additive FFT */
    compute_roots(error, sigma);

    /* Axis D — vectorised Forney error-value computation */
    compute_z_poly(z, sigma, deg, syndromes);
    compute_error_values_zvbc(error_values, z, error);
    correct_errors(cdw_bytes, error_values);

    memcpy(msg, cdw_bytes + (PARAM_G - 1), PARAM_K);
    memset_zero(cdw_bytes, sizeof cdw_bytes);
}

/* ── Encoder (scalar, unchanged from reference) ─────────────────────────────
 *
 * Copied verbatim from vendor/hqc-ref/src/ref/reed_solomon.c.
 * Required by vendor/hqc-ref/src/common/code.c.
 */
void reed_solomon_encode(uint64_t *cdw, const uint64_t *msg)
{
    size_t i, j, k;
    uint8_t gate_value = 0;

    uint16_t tmp[PARAM_G]            = {0};
    uint16_t PARAM_RS_POLY[]         = {RS_POLY_COEFS};
    uint8_t  msg_bytes[PARAM_K]      = {0};
    uint8_t  cdw_bytes[PARAM_N1]     = {0};

    memcpy(msg_bytes, msg, PARAM_K);

    for (i = 0; i < PARAM_K; ++i) {
        gate_value = msg_bytes[PARAM_K - 1 - i] ^ cdw_bytes[PARAM_N1 - PARAM_K - 1];

        for (j = 0; j < PARAM_G; ++j)
            tmp[j] = gf_mul(gate_value, PARAM_RS_POLY[j]);

        for (k = PARAM_N1 - PARAM_K - 1; k; --k)
            cdw_bytes[k] = cdw_bytes[k - 1] ^ tmp[k];

        cdw_bytes[0] = tmp[0];
    }

    memcpy(cdw_bytes + PARAM_N1 - PARAM_K, msg_bytes, PARAM_K);
    memcpy(cdw, cdw_bytes, PARAM_N1);
}
