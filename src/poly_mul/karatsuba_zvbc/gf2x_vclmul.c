/* src/poly_mul/karatsuba_zvbc/gf2x_vclmul.c
 *
 * Phase 1+ (Track A) — Vectorised Karatsuba with vclmul.vv / vclmulh.vv.
 *
 * This is the Zvbc-vectorised version of karatsuba_zbc/gf2x_clmul.c.
 * Two optimisation axes are applied:
 *
 *   Iter 1 — Vectorise leaf nodes:
 *     Replace scalar `clmul`+`clmulh` double-loop with `vclmul.vv` +
 *     `vclmulh.vv` using a splat (vmv.v.x) + vector-loop over b[].
 *     For THRESHOLD=16 and VLEN=256 (LMUL=4 → 16 uint64_t per group),
 *     the entire inner j-loop collapses to ONE vector iteration:
 *       scalar: 16×16 × ~8 instr ≈ 2048 instr/leaf
 *       vector: 16 × ~10 instr    ≈  160 instr/leaf  (~12.8× on leaf)
 *
 *   Iter 2 — Vectorise Karatsuba XOR combines:
 *     The XOR accumulation in the combine phase (z0, zmid, z2 arrays)
 *     uses vxor.vv with LMUL=4 (16 uint64_t per iteration), replacing
 *     scalar word-by-word loops.
 *
 * Provides the same interface as vendor/hqc-ref/src/ref/gf2x.c:
 *   void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b);
 *
 * Requires: -march includes zvbc (ratified RISC-V vector carry-less multiply).
 * Detected at compile time via HQC_POLYMUL_ZVBC=1 (set by CMakeLists.txt).
 */

#include <stdint.h>
#include <string.h>
#include <riscv_vector.h>
#include "parameters.h"

#ifndef HQC_POLYMUL_ZVBC
#  error "gf2x_vclmul.c must be compiled with -DHQC_POLYMUL_ZVBC=1"
#endif

/* ── Karatsuba threshold ──────────────────────────────────────────────────
 * Same value as karatsuba_zbc; keep the same recursion depth.
 */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif

/* ── Temporary buffer sizing ─────────────────────────────────────────────
 * Identical formula to karatsuba_zbc.
 */
#define TMP_BUFFER_WORDS (16 * VEC_N_SIZE_64)

/* ── RVV helper: dst[0..n-1] ^= src[0..n-1]  (vxor.vv, LMUL=4) ─────────
 * Used for the Karatsuba combine phases.
 * With VLEN=256: 16 uint64_t per vector iteration (LMUL=4).
 */
static void vxor_words(uint64_t * __restrict__ dst,
                       const uint64_t * __restrict__ src,
                       size_t n)
{
    size_t i = 0;
    while (i < n) {
        size_t vl = __riscv_vsetvl_e64m4(n - i);
        vuint64m4_t vd = __riscv_vle64_v_u64m4(dst + i, vl);
        vuint64m4_t vs = __riscv_vle64_v_u64m4(src + i, vl);
        vd = __riscv_vxor_vv_u64m4(vd, vs, vl);
        __riscv_vse64_v_u64m4(dst + i, vd, vl);
        i += vl;
    }
}

/* ── Vectorised leaf multiply: r = a × b (unreduced, 2n words) ──────────
 *
 * Iter 1: for each a[i], splat into a vector register, then vclmul.vv /
 * vclmulh.vv over b[0..n-1] in one vector sweep (LMUL=4, vl up to 16).
 *
 * Correctness: each contribution lo(a[i],b[j]) → r[i+j] and
 * hi(a[i],b[j]) → r[i+j+1] is XOR-accumulated independently.
 * Since XOR is commutative, the order of accumulation does not matter.
 * Adjacent stores (e.g. r[i+j+1] from vhi and r[(i+1)+j] from vlo of
 * the next outer iteration) both contribute correctly via sequential
 * load-XOR-store chains.
 */
static void vclmul_leaf(uint64_t * __restrict__ r,
                        const uint64_t * __restrict__ a,
                        const uint64_t * __restrict__ b,
                        size_t n)
{
    memset(r, 0, 2 * n * sizeof(uint64_t));

    for (size_t i = 0; i < n; i++) {
        uint64_t ai = a[i];
        size_t j = 0;
        while (j < n) {
            size_t vl = __riscv_vsetvl_e64m4(n - j);

            /* Splat scalar a[i] into all lanes. */
            vuint64m4_t vai = __riscv_vmv_v_x_u64m4(ai, vl);

            /* Load b[j..j+vl-1]. */
            vuint64m4_t vb = __riscv_vle64_v_u64m4(b + j, vl);

            /* Carry-less multiply: 64×64 → 128 bit per lane. */
            vuint64m4_t vlo = __riscv_vclmul_vv_u64m4(vai, vb, vl);
            vuint64m4_t vhi = __riscv_vclmulh_vv_u64m4(vai, vb, vl);

            /* XOR low half into r[i+j .. i+j+vl-1]. */
            vuint64m4_t vr0 = __riscv_vle64_v_u64m4(r + i + j, vl);
            vr0 = __riscv_vxor_vv_u64m4(vr0, vlo, vl);
            __riscv_vse64_v_u64m4(r + i + j, vr0, vl);

            /* XOR high half into r[i+j+1 .. i+j+vl].
             * The tail element r[i+j+vl] will be further updated by the
             * next j-iteration's vlo stores, which is correct. */
            vuint64m4_t vr1 = __riscv_vle64_v_u64m4(r + i + j + 1, vl);
            vr1 = __riscv_vxor_vv_u64m4(vr1, vhi, vl);
            __riscv_vse64_v_u64m4(r + i + j + 1, vr1, vl);

            j += vl;
        }
    }
}

/* ── Recursive Karatsuba with vectorised leaf and combines ───────────────
 * Structure identical to karatsuba_zbc; only the base case (vclmul_leaf)
 * and the combine XOR loops (vxor_words) differ.
 */
static void karatsuba_vclmul(uint64_t * __restrict__ r,
                              const uint64_t * __restrict__ a,
                              const uint64_t * __restrict__ b,
                              size_t n,
                              uint64_t * __restrict__ tmp)
{
    if (n <= KARATSUBA_THRESHOLD) {
        vclmul_leaf(r, a, b, n);
        return;
    }

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;   /* n1 == m or m+1 when n is odd */

    /* Slice temporary storage (8*n words at this level). */
    uint64_t *z0   = tmp;
    uint64_t *z2   = z0 + 2 * n;
    uint64_t *zmid = z2 + 2 * n;
    uint64_t *ta   = zmid + 2 * n;
    uint64_t *tb   = ta + n;
    uint64_t *child_buf = tmp + 8 * n;

    /* 1) z0 = a_lo * b_lo */
    karatsuba_vclmul(z0, a, b, n0, child_buf);

    /* 2) z2 = a_hi * b_hi */
    karatsuba_vclmul(z2, a + m, b + m, n1, child_buf);

    /* 3) ta = a_lo XOR a_hi,  tb = b_lo XOR b_hi  (handles odd n1). */
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }

    /* 4) zmid = ta * tb */
    karatsuba_vclmul(zmid, ta, tb, n1, child_buf);

    /* 5) Assemble result using vectorised XOR (Iter 2):
     *      r = z0  +  (zmid ^ z0 ^ z2) * x^m  +  z2 * x^(2m)   */
    memset(r, 0, 2 * n * sizeof(uint64_t));

    /* r[0..2*n0-1] ^= z0[0..2*n0-1] */
    vxor_words(r, z0, 2 * n0);

    /* r[2*m..2*m+2*n1-1] ^= z2[0..2*n1-1] */
    vxor_words(r + 2 * m, z2, 2 * n1);

    /* r[m..m+2*n1-1] ^= zmid ^ z0 ^ z2
     * z0 contribution is zero for i >= 2*n0 (handled by the ternary). */
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
    }
}

/* ── Modular reduction mod (x^PARAM_N - 1) ──────────────────────────────
 * Identical to karatsuba_zbc; the offset pattern makes full vectorisation
 * complex (sliding window), so we keep the scalar version.  Its share of
 * total instret is small (<5%).
 */
static void reduce(uint64_t * __restrict__ o,
                   const uint64_t * __restrict__ a)
{
    int shift = PARAM_N & 0x3F;
    int inv   = 64 - shift;

    for (size_t i = 0; i < VEC_N_SIZE_64; i++) {
        uint64_t r  = a[i + VEC_N_SIZE_64 - 1] >> shift;
        uint64_t carry = (shift == 0) ? 0 : (a[i + VEC_N_SIZE_64] << inv);
        o[i] = a[i] ^ r ^ carry;
    }
    o[VEC_N_SIZE_64 - 1] &= BITMASK(PARAM_N, 64);
}

/* ── Public entry point ──────────────────────────────────────────────────
 * Identical interface to vendor/hqc-ref/src/ref/gf2x.c and karatsuba_zbc.
 */
void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[TMP_BUFFER_WORDS];

    karatsuba_vclmul(unreduced, a, b, VEC_N_SIZE_64, tmp_buf);
    reduce(o, unreduced);
}
