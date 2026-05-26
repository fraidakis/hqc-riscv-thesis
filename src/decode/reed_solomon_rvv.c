/* src/decode/reed_solomon_rvv.c
 *
 * Phase 3b — RVV-accelerated Reed-Solomon decoder for HQC.
 *
 * Status:
 *   Axis A — compute_syndromes: DONE — vectorised via RVV log/exp table gathers.
 *            Pure RVV (no Zvbc/Zvbb dependency). Compiled with rv64gcv_zbc
 *            (scalar Zbc for gf_mul in fallback axes).
 *   Axis B — compute_elp (Berlekamp-Massey): scalar fallback (sequential by design).
 *   Axis C — compute_roots / fft_rec: scalar fallback.
 *   Axis D — compute_error_values (Forney): scalar fallback.
 *
 * Vectorisation strategy for Axis A (compute_syndromes):
 *
 *   The syndrome computation evaluates 2*PARAM_DELTA independent dot products
 *   over GF(2^8), each of length PARAM_N1-1:
 *
 *       syndromes[i] = cdw[0] ^ XOR_{j=1}^{N1-1} gf_mul(cdw[j], alpha_ij_pow[i][j-1])
 *
 *   GF(2^8) multiplication via RVV log/exp table lookups (SEW=16, LMUL=4):
 *     - Load cdw[j] and alpha_ij_pow[i][j] as uint16_t values.
 *     - Gather gf_log[cdw[j]] and gf_log[alpha[j]] via vluxei16 indexed loads.
 *     - Add the two log values (mod 255) to get log(product).
 *     - Gather gf_exp[log_sum] via vluxei16 to get the product.
 *     - Mask out lanes where either operand is zero (gf_mul(0,x) = 0).
 *     - XOR-reduce across lanes with vredxor.vs to accumulate into syndromes[i].
 *
 *   With VLEN=256, SEW=16, LMUL=4: vl = 256*4/16 = 64 elements per pass.
 *     L128: ceil(45/64)=1 pass  × 30 syndromes ≈ ~30 vector passes total.
 *     L192: ceil(55/64)=1 pass  × 32 syndromes ≈ ~32 vector passes total.
 *     L256: ceil(89/64)=2 passes × 58 syndromes ≈ ~116 vector passes total.
 *
 * All remaining sub-functions (ELP, roots, z-poly, error values, correction)
 * are copied verbatim from vendor/hqc-ref/src/ref/reed_solomon.c.  Once Axis B
 * and Axis C are implemented they will replace these copies.
 *
 * Interface (unchanged from the scalar reference):
 *   void reed_solomon_decode(uint64_t *msg, uint64_t *cdw);
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>

#include "reed_solomon.h"   /* alpha_ij_pow (level-specific) */
#include "gf.h"             /* gf_mul, gf_inverse, gf_exp, gf_log */
#include "fft.h"            /* fft, fft_retrieve_error_poly */
#include "parameters.h"     /* PARAM_DELTA, PARAM_N1, PARAM_M, PARAM_FFT, PARAM_K, PARAM_G */
#include "crypto_memset.h"  /* memset_zero */

/* ── Axis A: Vectorised syndrome computation (RVV log/exp gather) ────────────
 *
 * Computes 2*PARAM_DELTA syndromes using RVV indexed gather loads on the
 * gf_log[] and gf_exp[] tables for GF(2^8) multiplication.
 *
 * For each syndrome i:
 *   syndromes[i] = cdw[0] XOR (XOR_j gf_mul(cdw[j+1], alpha_ij_pow[i][j]))
 *
 * GF(2^8) multiply via log/exp:
 *   gf_mul(a, b) = (a == 0 || b == 0) ? 0 : gf_exp[(gf_log[a] + gf_log[b]) % 255]
 *
 * The gf_exp table has 258 entries (indices 0..257), so any sum of two logs
 * (each in [0,254]) fits within bounds without explicit modular reduction —
 * we use the identity: gf_exp[x % 255] == gf_exp[x] for x < 510 when the
 * table is extended with gf_exp[255]=1, gf_exp[256]=2, gf_exp[257]=4.
 * However, max(log_a + log_b) = 254+254 = 508 > 257, so we must reduce.
 * Reduction: if sum >= 255, subtract 255.  This is a single conditional
 * subtract implemented branchlessly with a mask.
 *
 * Inner loop vectorised over j; outer loop (i) remains scalar.
 *
 * @param[out] syndromes  Array of 2*PARAM_DELTA uint16_t syndrome values
 * @param[in]  cdw        Array of PARAM_N1 uint8_t codeword bytes
 */
static void compute_syndromes_rvv(uint16_t *syndromes, const uint8_t *cdw)
{
    for (size_t i = 0; i < 2u * PARAM_DELTA; i++) {
        uint16_t acc = (uint16_t)cdw[0];

        const uint8_t  *pa  = cdw + 1;                 /* cdw[1 .. N1-1]           */
        const uint16_t *pb  = alpha_ij_pow[i];          /* alpha_ij_pow[i][0..N1-2] */
        size_t          rem = PARAM_N1 - 1u;

        while (rem > 0) {
            size_t vl = __riscv_vsetvl_e16m4(rem);

            /* Load codeword bytes, zero-extend u8 → u16 for table indexing.  */
            vuint8m2_t   a8    = __riscv_vle8_v_u8m2(pa, vl);
            vuint16m4_t  a16   = __riscv_vzext_vf2_u16m4(a8, vl);

            /* Load precomputed alpha powers (already uint16_t, values 0..255).*/
            vuint16m4_t  b16   = __riscv_vle16_v_u16m4(pb, vl);

            /* Gather gf_log[a] and gf_log[b] via indexed loads.
             * Index = element_value * sizeof(uint16_t) = element_value * 2.
             * gf_log is uint16_t[256], so byte offset = value << 1.          */
            vuint16m4_t  idx_a = __riscv_vsll_vx_u16m4(a16, 1, vl);
            vuint16m4_t  idx_b = __riscv_vsll_vx_u16m4(b16, 1, vl);
            vuint16m4_t  log_a = __riscv_vluxei16_v_u16m4(gf_log, idx_a, vl);
            vuint16m4_t  log_b = __riscv_vluxei16_v_u16m4(gf_log, idx_b, vl);

            /* Sum of logs; reduce mod 255 branchlessly.
             * sum = log_a + log_b (max 254+254 = 508).
             * if sum >= 255: sum -= 255.                                      */
            vuint16m4_t  log_sum = __riscv_vadd_vv_u16m4(log_a, log_b, vl);
            vbool4_t     ge_mask = __riscv_vmsgeu_vx_u16m4_b4(log_sum, 255, vl);
            log_sum = __riscv_vsub_vx_u16m4_mu(ge_mask, log_sum, log_sum, 255, vl);

            /* Gather gf_exp[log_sum] to get the GF product.
             * gf_exp is uint16_t[258], byte offset = log_sum << 1.           */
            vuint16m4_t  idx_exp = __riscv_vsll_vx_u16m4(log_sum, 1, vl);
            vuint16m4_t  prod    = __riscv_vluxei16_v_u16m4(gf_exp, idx_exp, vl);

            /* Zero-mask: gf_mul(0, x) = gf_mul(x, 0) = 0.
             * Set product to 0 where either operand is zero.
             * nz = (a != 0) AND (b != 0).
             * Where nz is false (either operand zero), force product to 0.
             * vmerge(op1, op2, mask, vl): result[i] = mask[i] ? op2 : op1.
             * We want: nz ? prod : 0.                                        */
            vbool4_t     a_nz   = __riscv_vmsne_vx_u16m4_b4(a16, 0, vl);
            vbool4_t     b_nz   = __riscv_vmsne_vx_u16m4_b4(b16, 0, vl);
            vbool4_t     nz     = __riscv_vmand_mm_b4(a_nz, b_nz, vl);
            vuint16m4_t  zero16 = __riscv_vmv_v_x_u16m4(0, vl);
            prod = __riscv_vmerge_vvm_u16m4(zero16, prod, nz, vl);

            /* XOR-reduce this pass into the running accumulator.             */
            vuint16m1_t  zero_v = __riscv_vmv_v_x_u16m1(0u, 1);
            vuint16m1_t  red    = __riscv_vredxor_vs_u16m4_u16m1(prod, zero_v, vl);
            acc ^= __riscv_vmv_x_s_u16m1_u16(red);

            pa  += vl;
            pb  += vl;
            rem -= vl;
        }

        syndromes[i] = acc;
    }
}

/* ── Scalar fallback sub-functions ──────────────────────────────────────────
 *
 * Copied verbatim from vendor/hqc-ref/src/ref/reed_solomon.c.
 * These will be replaced incrementally as Axes B, C, D are implemented.
 */

static uint16_t compute_elp(uint16_t *sigma, const uint16_t *syndromes)
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
    uint16_t deg_X, deg_X_sigma_p, dd, mu, i;

    sigma[0] = 1;
    for (mu = 0; mu < 2u * PARAM_DELTA; ++mu) {
        memcpy(sigma_copy, sigma, 2u * PARAM_DELTA);
        deg_sigma_copy = deg_sigma;

        dd = gf_mul(d, gf_inverse(d_p));

        for (i = 1; i <= mu + 1u && i <= PARAM_DELTA; ++i)
            sigma[i] ^= gf_mul(dd, X_sigma_p[i]);

        deg_X        = mu - pp;
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
        for (i = PARAM_DELTA; i; --i)
            X_sigma_p[i] = (mask12 & sigma_copy[i - 1]) ^ (~mask12 & X_sigma_p[i - 1]);

        deg_sigma_p ^= mask12 & (deg_sigma_copy ^ deg_sigma_p);
        d = syndromes[mu + 1];
        for (i = 1; i <= mu + 1u && i <= PARAM_DELTA; ++i)
            d ^= gf_mul(sigma[i], syndromes[mu + 1u - i]);
    }
    return deg_sigma;
}

static void compute_roots(uint8_t *error, uint16_t *sigma)
{
    uint16_t w[1 << PARAM_M] = {0};
    fft(w, sigma, PARAM_DELTA + 1);
    fft_retrieve_error_poly(error, w);
}

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

static void compute_error_values(uint16_t *error_values,
                                 const uint16_t *z, const uint8_t *error)
{
    uint16_t beta_j[PARAM_DELTA]     = {0};
    uint16_t e_j[PARAM_DELTA]        = {0};
    uint16_t delta_counter, delta_real_value, found;
    uint16_t mask1, mask2, tmp1, tmp2, inverse, inverse_power_j;

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

    for (size_t i = 0; i < PARAM_DELTA; ++i) {
        tmp1 = 1; tmp2 = 1;
        inverse        = gf_inverse(beta_j[i]);
        inverse_power_j = 1;
        for (size_t j = 1; j <= PARAM_DELTA; ++j) {
            inverse_power_j = gf_mul(inverse_power_j, inverse);
            tmp1 ^= gf_mul(inverse_power_j, z[j]);
        }
        for (size_t k = 1; k < PARAM_DELTA; ++k)
            tmp2 = gf_mul(tmp2, 1u ^ gf_mul(inverse, beta_j[(i + k) % PARAM_DELTA]));
        mask1  = (uint16_t)(((int16_t)i - delta_real_value) >> 15);
        e_j[i] = mask1 & gf_mul(tmp1, gf_inverse(tmp2));
    }

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

static void correct_errors(uint8_t *cdw, const uint16_t *error_values)
{
    for (size_t i = 0; i < PARAM_N1; ++i)
        cdw[i] ^= (uint8_t)error_values[i];
}

/* ── Public entry point ──────────────────────────────────────────────────────
 *
 * Drop-in replacement for reed_solomon_decode() in reed_solomon.c.
 * Axis A (syndromes) is vectorised; all other axes remain scalar for now.
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

    /* Axis B (scalar fallback) — error locator polynomial via Berlekamp-Massey */
    deg = compute_elp(sigma, syndromes);

    /* Axis C (scalar fallback) — root finding via additive FFT */
    compute_roots(error, sigma);

    /* Axis D (scalar fallback) — Forney formula */
    compute_z_poly(z, sigma, deg, syndromes);
    compute_error_values(error_values, z, error);
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
