/* src/decode/fft_zvbc.c
 *
 * Iterative breadth-first Gao-Mateer additive FFT implementation.
 * Replaces the recursive fft_rec() with a level-by-level BFS traversal.
 * All working arrays are compile-time-constant-sized (no VLA, no heap).
 * No data-dependent branches - constant-time execution.
 *
 * Interface: drop-in replacement for vendor fft.c
 */

#include "fft.h"
#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "gf.h"
#include "parameters.h"

/* ── GF(2^8) inline Barrett reduction (shared with reed_solomon_zvbc.c) ─────
 *
 * Reduces a vector of 14-bit carry-less products (each in bits [13:0] of a
 * 64-bit lane) modulo PARAM_GF_POLY = 0x11D (x^8+x^4+x^3+x^2+1).
 * Single-pass Barrett via vclmul: 6 vector instructions.
 * Constant-time: no data-dependent branches.
 */
static inline vuint64m4_t gf_reduce_vec_u64m4(vuint64m4_t v, size_t vl)
{
    /*
     * Single-pass Barrett reduction in GF(2)[x] mod 0x11D.
     *
     * Given v with 14-bit carry-less products (degree ≤ 13), reduce mod
     * p(x) = x^8 + x^4 + x^3 + x^2 + 1 (0x11D) using Barrett's method:
     *
     *   high = v >> 8                    (overflow bits [13:8], degree ≤ 5)
     *   q    = clmul(high, μ) >> 8       (quotient estimate)
     *   corr = clmul(q, poly)            (correction = q × poly)
     *   r    = (v XOR corr) & 0xFF       (remainder in [0, 255])
     *
     * Barrett reciprocal: μ = floor(x^16 / poly) = 0x11C in GF(2)[x].
     * However, using poly (0x11D) as μ also produces correct results for
     * all 14-bit inputs because the +1 difference (bit 0) does not affect
     * the quotient after the >> 8 shift for operands of degree ≤ 5.
     * This allows reusing a single constant vector for both clmul operations.
     *
     * Instruction count: 6 vector instructions (+ 1 vmv_v_x broadcast that
     * can be hoisted out of loops by the compiler).
     *
     * Constant-time: no data-dependent branches; all operations are
     * fixed-latency regardless of input values.
     */
    const uint64_t BARRETT_POLY = 0x11Du;

    vuint64m4_t mu_vec = __riscv_vmv_v_x_u64m4(BARRETT_POLY, vl);

    /* Step 1: Extract overflow bits [13:8] */
    vuint64m4_t high = __riscv_vsrl_vx_u64m4(v, 8, vl);

    /* Step 2: Quotient estimate = (high × μ) >> 8 */
    vuint64m4_t q = __riscv_vclmul_vv_u64m4(high, mu_vec, vl);
    q = __riscv_vsrl_vx_u64m4(q, 8, vl);

    /* Step 3: Correction = q × poly */
    vuint64m4_t correction = __riscv_vclmul_vv_u64m4(q, mu_vec, vl);

    /* Step 4–5: XOR correction and mask to 8 bits */
    v = __riscv_vxor_vv_u64m4(v, correction, vl);
    v = __riscv_vand_vx_u64m4(v, 0xFFu, vl);

    return v;
}

/* ── Vectorized XOR-broadcast combination step ───────────────────────────────
 *
 * Computes dst[k] = src[k] ^ scalar_val for k = 0..len-1 using contiguous
 * vle16 / vxor.vx / vse16 pipeline.
 *
 * VLA loop using __riscv_vsetvl_e16m4 tiles across the full sub-array,
 * adapting to hardware VLEN.  Constant-time: loop bounds depend only on
 * public parameters (sub-array length at each tree level).
 */
static inline void xor_broadcast_zvbc(uint16_t *dst, const uint16_t *src,
                                      size_t len, uint16_t scalar_val)
{
    size_t rem = len;
    while (rem > 0) {
        size_t vl = __riscv_vsetvl_e16m4(rem);
        vuint16m4_t v = __riscv_vle16_v_u16m4(src, vl);
        vuint16m4_t r = __riscv_vxor_vx_u16m4(v, scalar_val, vl);
        __riscv_vse16_v_u16m4(dst, r, vl);
        src += vl;
        dst += vl;
        rem -= vl;
    }
}

/* ── Vectorized vector-vector XOR (in-place) ────────────────────────────────
 *
 * Computes dst[k] ^= src[k] for k = 0..len-1 using contiguous vle16 / vxor.vv
 * / vse16 pipeline.  Used for the bottom-up combine step
 *   w[k + i] = v[i] ^ w[i]      (after memcpy of v into w[k..2k-1])
 *
 * VLA loop using __riscv_vsetvl_e16m4 adapts to hardware VLEN.
 * Constant-time: loop bounds depend only on public sub-array length.
 */
static inline void xor_vv_zvbc(uint16_t *dst, const uint16_t *src, size_t len)
{
    size_t rem = len;
    while (rem > 0) {
        size_t vl = __riscv_vsetvl_e16m4(rem);
        vuint16m4_t a = __riscv_vle16_v_u16m4(dst, vl);
        vuint16m4_t b = __riscv_vle16_v_u16m4(src, vl);
        vuint16m4_t r = __riscv_vxor_vv_u16m4(a, b, vl);
        __riscv_vse16_v_u16m4(dst, r, vl);
        dst += vl;
        src += vl;
        rem -= vl;
    }
}

/* ── Vectorized gammas-sums combination ──────────────────────────────────────
 *
 * Computes out[i] = u[i] ^ gf_mul(gs[i], v[i]) for i = 0..count-1 using
 * vclmul.vv + gf_reduce_vec_u64m4 + vxor.vv pipeline.
 *
 * This is the core of the FFT butterfly combining step:
 *   w[i] = u[i] ^ gf_mul(gammas_sums[i], v[i])
 *
 * VLA loop using __riscv_vsetvl_e64m4 adapts to hardware VLEN.
 * Constant-time: all operations have fixed latency independent of values.
 */
static inline void gammas_sums_combine_zvbc(uint16_t *out,
                                            const uint16_t *u,
                                            const uint16_t *gs,
                                            const uint16_t *v,
                                            size_t count)
{
    const uint16_t *pu = u;
    const uint16_t *pg = gs;
    const uint16_t *pv = v;
    uint16_t *po = out;
    size_t rem = count;

    while (rem > 0) {
        size_t vl = __riscv_vsetvl_e64m4(rem);

        /* Load gammas_sums[i], zero-extend u16 → u64 */
        vuint16m1_t gs16 = __riscv_vle16_v_u16m1(pg, vl);
        vuint64m4_t gs64 = __riscv_vzext_vf4_u64m4(gs16, vl);

        /* Load v[i], zero-extend u16 → u64 */
        vuint16m1_t v16 = __riscv_vle16_v_u16m1(pv, vl);
        vuint64m4_t v64 = __riscv_vzext_vf4_u64m4(v16, vl);

        /* Carry-less multiply: gf_mul(gs[i], v[i]) */
        vuint64m4_t prod = __riscv_vclmul_vv_u64m4(gs64, v64, vl);
        prod = gf_reduce_vec_u64m4(prod, vl);

        /* Load u[i], zero-extend u16 → u64 */
        vuint16m1_t u16 = __riscv_vle16_v_u16m1(pu, vl);
        vuint64m4_t u64 = __riscv_vzext_vf4_u64m4(u16, vl);

        /* XOR: u[i] ^ gf_mul(gs[i], v[i]) */
        vuint64m4_t res64 = __riscv_vxor_vv_u64m4(u64, prod, vl);

        /* Narrow u64m4 → u32m2 → u16m1 and store */
        vuint32m2_t res32 = __riscv_vnsrl_wx_u32m2(res64, 0, vl);
        vuint16m1_t res16 = __riscv_vnsrl_wx_u16m1(res32, 0, vl);
        __riscv_vse16_v_u16m1(po, res16, vl);

        pu += vl;
        pg += vl;
        pv += vl;
        po += vl;
        rem -= vl;
    }
}

/* ── Vectorized gammas-sums combination with broadcast scalar ────────────────
 *
 * Computes out[i] = u[i] ^ gf_mul(gs[i], scalar) for i = 0..count-1 using
 * vclmul broadcast + gf_reduce_vec_u64m4 + vxor.vv pipeline.
 *
 * Used in the bottom-up combine step when v is a constant (sc <= 3 case):
 *   w[i] = u[i] ^ gf_mul(gammas_sums[i], f1_0)
 *
 * VLA loop using __riscv_vsetvl_e64m4 adapts to hardware VLEN.
 * Constant-time: all operations have fixed latency independent of values.
 */
static inline void gammas_sums_combine_scalar_zvbc(uint16_t *out,
                                                   const uint16_t *u,
                                                   const uint16_t *gs,
                                                   uint16_t scalar,
                                                   size_t count)
{
    const uint16_t *pu = u;
    const uint16_t *pg = gs;
    uint16_t *po = out;
    size_t rem = count;

    while (rem > 0) {
        size_t vl = __riscv_vsetvl_e64m4(rem);

        /* Load gammas_sums[i], zero-extend u16 → u64 */
        vuint16m1_t gs16 = __riscv_vle16_v_u16m1(pg, vl);
        vuint64m4_t gs64 = __riscv_vzext_vf4_u64m4(gs16, vl);

        /* Broadcast scalar to all lanes */
        vuint64m4_t s64 = __riscv_vmv_v_x_u64m4((uint64_t)(scalar & 0xFFu), vl);

        /* Carry-less multiply: gf_mul(gs[i], scalar) */
        vuint64m4_t prod = __riscv_vclmul_vv_u64m4(gs64, s64, vl);
        prod = gf_reduce_vec_u64m4(prod, vl);

        /* Load u[i], zero-extend u16 → u64 */
        vuint16m1_t u16 = __riscv_vle16_v_u16m1(pu, vl);
        vuint64m4_t u64 = __riscv_vzext_vf4_u64m4(u16, vl);

        /* XOR: u[i] ^ gf_mul(gs[i], scalar) */
        vuint64m4_t res64 = __riscv_vxor_vv_u64m4(u64, prod, vl);

        /* Narrow u64m4 → u32m2 → u16m1 and store */
        vuint32m2_t res32 = __riscv_vnsrl_wx_u32m2(res64, 0, vl);
        vuint16m1_t res16 = __riscv_vnsrl_wx_u16m1(res32, 0, vl);
        __riscv_vse16_v_u16m1(po, res16, vl);

        pu += vl;
        pg += vl;
        po += vl;
        rem -= vl;
    }
}

/* ── Vectorized beta-scaling pass using Zvbc vclmul.vv ──────────────────────
 *
 * Computes f[i] = gf_mul(beta^i, f[i]) for i = 1..n-1 using the
 *   vle16 → vzext_vf4_u64m4 → vclmul.vv → gf_reduce_vec_u64m4 →
 *   vnsrl narrow → vse16
 * pipeline (same lane-packing strategy as Axes A, B, D).
 *
 * Beta powers are precomputed scalar-style into a stack array of bounded
 * size (≤ 1 << (PARAM_FFT - 1) = 16 elements at HQC-256).  The vectorized
 * element-wise GF(2^8) multiply then runs across the array via a VLA loop
 * using __riscv_vsetvl_e64m4 to adapt to the hardware VLEN.
 *
 * Constant-time: the gf_mul(1, x) = x identity case (beta == 1) is processed
 * unconditionally, eliminating the data-independent control-flow guard from
 * the recursive reference.  All vector ops have fixed latency independent of
 * coefficient values.
 */
static inline void beta_scale_zvbc(uint16_t *f, size_t n, uint16_t beta)
{
    uint16_t beta_pows[1 << (PARAM_FFT - 1)];

    /* Precompute beta^1, beta^2, ..., beta^(n-1) (scalar chain — unavoidable) */
    uint16_t beta_pow = 1;
    for (size_t i = 1; i < n; ++i) {
        beta_pow = gf_mul(beta_pow, beta);
        beta_pows[i] = beta_pow;
    }

    /* Vectorized element-wise GF multiply: f[i] *= beta_pows[i] for i=1..n-1 */
    const uint16_t *pb  = beta_pows + 1;
    uint16_t       *pf  = f + 1;
    size_t          rem = (n > 0) ? n - 1 : 0;

    while (rem > 0) {
        size_t vl = __riscv_vsetvl_e64m4(rem);

        /* Load f[i], zero-extend u16 → u64 lane */
        vuint16m1_t f16 = __riscv_vle16_v_u16m1(pf, vl);
        vuint64m4_t f64 = __riscv_vzext_vf4_u64m4(f16, vl);

        /* Load beta_pows[i], zero-extend u16 → u64 lane */
        vuint16m1_t b16 = __riscv_vle16_v_u16m1(pb, vl);
        vuint64m4_t b64 = __riscv_vzext_vf4_u64m4(b16, vl);

        /* Carry-less multiply + inline two-pass Barrett reduction */
        vuint64m4_t prod = __riscv_vclmul_vv_u64m4(f64, b64, vl);
        prod = gf_reduce_vec_u64m4(prod, vl);

        /* Narrow u64m4 → u32m2 → u16m1 and store */
        vuint32m2_t r32 = __riscv_vnsrl_wx_u32m2(prod, 0, vl);
        vuint16m1_t r16 = __riscv_vnsrl_wx_u16m1(r32, 0, vl);
        __riscv_vse16_v_u16m1(pf, r16, vl);

        pf  += vl;
        pb  += vl;
        rem -= vl;
    }
}

static void compute_fft_betas(uint16_t *betas) {
    for (size_t i = 0; i < PARAM_M - 1; ++i)
        betas[i] = 1 << (PARAM_M - 1 - i);
}

static void compute_subset_sums(uint16_t *subset_sums,
                                const uint16_t *set, uint16_t set_size) {
    subset_sums[0] = 0;
    for (uint16_t i = 0; i < set_size; ++i)
        for (uint16_t j = 0; j < (1 << i); ++j)
            subset_sums[(1 << i) + j] = set[i] ^ subset_sums[j];
}

static void radix(uint16_t *f0, uint16_t *f1, const uint16_t *f, uint32_t m_f);
static void radix_big(uint16_t *f0, uint16_t *f1, const uint16_t *f, uint32_t m_f);

static void radix(uint16_t *f0, uint16_t *f1, const uint16_t *f, uint32_t m_f) {
    switch (m_f) {
    case 4:
        f0[4]=f[8]^f[12]; f0[6]=f[12]^f[14]; f0[7]=f[14]^f[15];
        f1[5]=f[11]^f[13]; f1[6]=f[13]^f[14]; f1[7]=f[15];
        f0[5]=f[10]^f[12]^f1[5]; f1[4]=f[9]^f[13]^f0[5];
        f0[0]=f[0]; f1[3]=f[7]^f[11]^f[15];
        f0[3]=f[6]^f[10]^f[14]^f1[3];
        f0[2]=f[4]^f0[4]^f0[3]^f1[3];
        f1[1]=f[3]^f[5]^f[9]^f[13]^f1[3];
        f1[2]=f[3]^f1[1]^f0[3];
        f0[1]=f[2]^f0[2]^f1[1]; f1[0]=f[1]^f0[1];
        break;
    case 3:
        f0[0]=f[0]; f0[2]=f[4]^f[6]; f0[3]=f[6]^f[7];
        f1[1]=f[3]^f[5]^f[7]; f1[2]=f[5]^f[6]; f1[3]=f[7];
        f0[1]=f[2]^f0[2]^f1[1]; f1[0]=f[1]^f0[1];
        break;
    case 2:
        f0[0]=f[0]; f0[1]=f[2]^f[3];
        f1[0]=f[1]^f0[1]; f1[1]=f[3];
        break;
    case 1:
        f0[0]=f[0]; f1[0]=f[1];
        break;
    default:
        radix_big(f0, f1, f, m_f);
        break;
    }
}

static void radix_big(uint16_t *f0, uint16_t *f1, const uint16_t *f, uint32_t m_f) {
    uint16_t Q[2*(1<<(PARAM_FFT-2))+1] = {0};
    uint16_t R[2*(1<<(PARAM_FFT-2))+1] = {0};
    uint16_t Q0[1<<(PARAM_FFT-2)] = {0};
    uint16_t Q1[1<<(PARAM_FFT-2)] = {0};
    uint16_t R0[1<<(PARAM_FFT-2)] = {0};
    uint16_t R1[1<<(PARAM_FFT-2)] = {0};
    size_t n = (size_t)1 << (m_f - 2);
    memcpy(Q, f + 3*n, 2*n);
    memcpy(Q + n, f + 3*n, 2*n);
    memcpy(R, f, 4*n);
    for (size_t i = 0; i < n; ++i) {
        Q[i] ^= f[2*n + i];
        R[n + i] ^= Q[i];
    }
    radix(Q0, Q1, Q, m_f - 1);
    radix(R0, R1, R, m_f - 1);
    memcpy(f0, R0, 2*n);
    memcpy(f0 + n, Q0, 2*n);
    memcpy(f1, R1, 2*n);
    memcpy(f1 + n, Q1, 2*n);
}

/* Pre-computed per-level FFT tables for iterative traversal. */
typedef struct {
    uint16_t level_betas[PARAM_FFT - 1][PARAM_M - 1];
    uint16_t gammas_sums[PARAM_FFT - 1][1 << (PARAM_M - 2)];
} fft_tables_t;

static void precompute_tables(fft_tables_t *tables,
                              const uint16_t *init_betas,
                              uint8_t m_init) {
    uint16_t cur_betas[PARAM_M - 1];
    uint16_t gammas[PARAM_M - 1];
    uint16_t next_betas[PARAM_M - 1];

    memcpy(cur_betas, init_betas, m_init * sizeof(uint16_t));

    for (uint32_t level = 0; level < (uint32_t)(PARAM_FFT - 2); ++level) {
        uint8_t m = m_init - (uint8_t)level;
        memcpy(tables->level_betas[level], cur_betas, m * sizeof(uint16_t));

        uint16_t inv_last = gf_inverse(cur_betas[m - 1]);
        for (size_t i = 0; i + 1 < m; ++i)
            gammas[i] = gf_mul(cur_betas[i], inv_last);
        compute_subset_sums(tables->gammas_sums[level], gammas, m - 1);

        memset(next_betas, 0, sizeof(next_betas));
        for (size_t i = 0; i + 1 < m; ++i)
            next_betas[i] = gf_square(gammas[i]) ^ gammas[i];
        memcpy(cur_betas, next_betas, (m - 1) * sizeof(uint16_t));
    }

    uint8_t m_leaf = m_init - (uint8_t)(PARAM_FFT - 2);
    memcpy(tables->level_betas[PARAM_FFT - 2], cur_betas,
           m_leaf * sizeof(uint16_t));
}

#define FFT_NUM_LEVELS (PARAM_FFT - 1)
#define FFT_LEVEL_SIZE (1 << (PARAM_FFT - 1))

static void fft_iterative(uint16_t *w, const uint16_t *f_in,
                           size_t f_coeffs, uint8_t m_init,
                           uint32_t m_f_init,
                           const fft_tables_t *tables) {
    uint16_t levels[FFT_NUM_LEVELS][FFT_LEVEL_SIZE];
    size_t coeffs[FFT_NUM_LEVELS][1 << (PARAM_FFT - 1)];

    memset(levels, 0, sizeof(levels));
    memset(coeffs, 0, sizeof(coeffs));

    size_t init_size = (size_t)1 << m_f_init;
    memcpy(levels[0], f_in, init_size * sizeof(uint16_t));
    coeffs[0][0] = f_coeffs;

    /* Pass 1 (top-down): beta-scale + radix at each level */
    for (uint32_t level = 0; level < m_f_init - 1; ++level) {
        uint32_t num_segs = 1u << level;
        uint32_t seg_size = 1u << (m_f_init - level);
        uint32_t child_size = seg_size >> 1;
        uint8_t  m = m_init - (uint8_t)level;
        const uint16_t *betas = tables->level_betas[level];
        uint16_t beta_last = betas[m - 1];

        for (uint32_t seg = 0; seg < num_segs; ++seg) {
            uint16_t *seg_ptr = &levels[level][seg * seg_size];

            /* Vectorized beta-scaling pass: seg_ptr[i] = gf_mul(beta_last^i, seg_ptr[i])
             * for i = 1..seg_size-1.  Constant-time — unconditional execution
             * (the beta == 1 identity case is a vector-lane-wide no-op since
             * gf_mul(1, x) = x, with fixed latency independent of values).
             */
            beta_scale_zvbc(seg_ptr, seg_size, beta_last);

            uint16_t *child0 = &levels[level+1][(2*seg) * child_size];
            uint16_t *child1 = &levels[level+1][(2*seg+1) * child_size];
            radix(child0, child1, seg_ptr, m_f_init - level);

            size_t sc = coeffs[level][seg];
            coeffs[level+1][2*seg]   = (sc + 1) / 2;
            coeffs[level+1][2*seg+1] = sc / 2;
        }
    }

    /* Leaf base case: XOR-broadcast */
    uint32_t leaf_level = m_f_init - 1;
    uint32_t num_leaves = 1u << leaf_level;
    uint8_t  m_leaf = m_init - (uint8_t)leaf_level;
    uint32_t leaf_out_size = 1u << m_leaf;
    const uint16_t *leaf_betas = tables->level_betas[leaf_level];
    uint16_t leaf_w[1 << (PARAM_M - 1)];
    uint16_t tmp_betas[PARAM_M - 1];

    memset(leaf_w, 0, sizeof(leaf_w));

    for (uint32_t seg = 0; seg < num_leaves; ++seg) {
        uint16_t *seg_ptr = &levels[leaf_level][seg * 2];
        uint16_t *out = &leaf_w[seg * leaf_out_size];

        for (uint8_t i = 0; i < m_leaf; ++i)
            tmp_betas[i] = gf_mul(leaf_betas[i], seg_ptr[1]);

        out[0] = seg_ptr[0];
        size_t x = 1;
        for (uint8_t j = 0; j < m_leaf; ++j) {
            /* Vectorized XOR-broadcast: out[x..2x-1] = out[0..x-1] ^ tmp_betas[j]
             * Tiles via VLA loop in xor_broadcast_zvbc; falls back to scalar
             * vle16 / vxor.vx / vse16 for x == 1 (single-element trivial case).
             */
            xor_broadcast_zvbc(out + x, out, x, tmp_betas[j]);
            x <<= 1;
        }
    }

    /* Pass 2 (bottom-up): combine children outputs */
    uint16_t w_cur[1 << (PARAM_M - 1)];
    uint16_t w_parent[1 << (PARAM_M - 1)];

    memcpy(w_cur, leaf_w, (1u << m_init) * sizeof(uint16_t));

    for (int32_t level = (int32_t)leaf_level - 1; level >= 0; --level) {
        uint32_t num_segs_this = 1u << level;
        uint8_t  m = m_init - (uint8_t)level;
        uint32_t out_size = 1u << m;
        uint32_t k = 1u << (m - 1);
        const uint16_t *gs = tables->gammas_sums[level];

        memset(w_parent, 0, (1u << m_init) * sizeof(uint16_t));

        for (uint32_t seg = 0; seg < num_segs_this; ++seg) {
            const uint16_t *u = &w_cur[(2*seg) * k];
            const uint16_t *v = &w_cur[(2*seg+1) * k];
            uint16_t *out = &w_parent[seg * out_size];
            size_t sc = coeffs[level][seg];

            if (sc <= 3) {
                /* f1 is constant — broadcast f1_0 in vector ops */
                uint16_t f1_0 = levels[level+1][(2*seg+1) *
                    ((uint32_t)1 << (m_f_init - 1 - level))];
                out[0] = u[0];
                out[k] = u[0] ^ f1_0;

                /* out[i] = u[i] ^ gf_mul(gs[i], f1_0) for i=1..k-1
                 *   → vclmul.vv broadcast + reduce + vxor.vv pipeline.
                 */
                gammas_sums_combine_scalar_zvbc(out + 1, u + 1, gs + 1,
                                                f1_0, k - 1);
                /* out[k+i] = out[i] ^ f1_0 for i=1..k-1
                 *   → vle16 / vxor.vx / vse16 pipeline.
                 */
                xor_broadcast_zvbc(out + k + 1, out + 1, k - 1, f1_0);
            } else {
                /* General case: v varies across lanes */
                memcpy(out + k, v, k * sizeof(uint16_t));
                out[0] = u[0];
                out[k] ^= u[0];

                /* out[i] = u[i] ^ gf_mul(gs[i], v[i]) for i=1..k-1
                 *   → vclmul.vv + reduce + vxor.vv pipeline.
                 */
                gammas_sums_combine_zvbc(out + 1, u + 1, gs + 1, v + 1, k - 1);
                /* out[k+i] ^= out[i] for i=1..k-1   (out[k+i] already has v[i])
                 *   → vle16 / vxor.vv / vse16 pipeline.
                 */
                xor_vv_zvbc(out + k + 1, out + 1, k - 1);
            }
        }
        memcpy(w_cur, w_parent, (1u << m_init) * sizeof(uint16_t));
    }

    memcpy(w, w_cur, (1u << m_init) * sizeof(uint16_t));
}

/* Public API */

void fft(uint16_t *w, const uint16_t *f, size_t f_coeffs) {
    uint16_t betas[PARAM_M - 1] = {0};
    uint16_t betas_sums[1 << (PARAM_M - 1)] = {0};
    uint16_t f0[1 << (PARAM_FFT - 1)] = {0};
    uint16_t f1[1 << (PARAM_FFT - 1)] = {0};
    uint16_t deltas[PARAM_M - 1] = {0};
    uint16_t u[1 << (PARAM_M - 1)] = {0};
    uint16_t v[1 << (PARAM_M - 1)] = {0};
    fft_tables_t tables;
    size_t i, k;

    compute_fft_betas(betas);
    compute_subset_sums(betas_sums, betas, PARAM_M - 1);
    radix(f0, f1, f, PARAM_FFT);

    for (i = 0; i < PARAM_M - 1; ++i)
        deltas[i] = gf_square(betas[i]) ^ betas[i];

    memset(&tables, 0, sizeof(tables));
    precompute_tables(&tables, deltas, PARAM_M - 1);

    fft_iterative(u, f0, (f_coeffs + 1) / 2,
                  PARAM_M - 1, PARAM_FFT - 1, &tables);
    fft_iterative(v, f1, f_coeffs / 2,
                  PARAM_M - 1, PARAM_FFT - 1, &tables);

    k = 1 << (PARAM_M - 1);
    memcpy(w + k, v, 2 * k);
    w[0] = u[0];
    w[k] ^= u[0];

    /* Top-level (root tree-level) gammas-sums combination — VLA tiled.
     *
     *   w[i]   = u[i] ^ gf_mul(betas_sums[i], v[i])     for i = 1..k-1
     *   w[k+i] = v[i] ^ w[i]                            for i = 1..k-1
     *
     * The first line is a vclmul.vv + Barrett-reduce + vxor.vv pipeline
     * (gammas_sums_combine_zvbc). The second line is an in-place vle16 /
     * vxor.vv / vse16 pipeline (xor_vv_zvbc); w[k+i] already holds v[i]
     * via the memcpy above, so XORing w[i] into it produces v[i] ^ w[i].
     *
     * Both loops are constant-time and vector-length-agnostic.
     */
    gammas_sums_combine_zvbc(w + 1, u + 1, betas_sums + 1, v + 1, k - 1);
    xor_vv_zvbc(w + k + 1, w + 1, k - 1);
}

void fft_retrieve_error_poly(uint8_t *error, const uint16_t *w) {
    uint16_t gammas[PARAM_M - 1] = {0};
    uint16_t gammas_sums[1 << (PARAM_M - 1)] = {0};
    uint16_t k;
    size_t i, index;

    compute_fft_betas(gammas);
    compute_subset_sums(gammas_sums, gammas, PARAM_M - 1);

    k = 1 << (PARAM_M - 1);
    error[0] ^= 1 ^ ((uint16_t)-w[0] >> 15);
    error[0] ^= 1 ^ ((uint16_t)-w[k] >> 15);

    for (i = 1; i < k; ++i) {
        index = PARAM_GF_MUL_ORDER - gf_log[gammas_sums[i]];
        error[index] ^= 1 ^ ((uint16_t)-w[i] >> 15);
        index = PARAM_GF_MUL_ORDER - gf_log[gammas_sums[i] ^ 1];
        error[index] ^= 1 ^ ((uint16_t)-w[k + i] >> 15);
    }
}
