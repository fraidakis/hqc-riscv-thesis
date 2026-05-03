/* src/poly_mul/hybrid_zbc/hybrid.c
 *
 * Phase 1d — Advanced Hybrid FAFFT-Karatsuba polynomial multiplication.
 *
 * ═══════════════════════════════════════════════════════════════════════
 * ALGORITHM — 2-Way FAFFT-Karatsuba Split (Jang et al. 2026)
 * ═══════════════════════════════════════════════════════════════════════
 * Phase 1c showed that wrapping FAFFT sub-problems in a 3-way recursive
 * Karatsuba degrades monotonically: each Karatsuba level triples the
 * number of FAFFT calls while the fixed-size kernels do not shrink.
 *
 * Phase 1d replaces the 3-way recursive scheme with a single 2-way split
 * at a power-of-2 boundary X = x^(2^d), chosen so that:
 *   (a) The large sub-products (C0, C_mid) fit in a SMALLER FAFFT domain
 *       than the full polynomial, eliminating padding overhead.
 *   (b) The residual hi×hi product is tiny and handled by scalar Karatsuba.
 *
 * For each security level the split point (SPLIT_WORDS) is chosen as the
 * largest power of 2 that is strictly less than VEC_N_SIZE_64:
 *   HQC-128: n=17669 → VEC_N_SIZE_64=277 → SPLIT=256 (2^14 bits), HI=21
 *   HQC-192: n=35851 → VEC_N_SIZE_64=561 → SPLIT=512 (2^15 bits), HI=49
 *   HQC-256: n=90501 → VEC_N_SIZE_64=1415 → SPLIT=1024(2^16 bits), HI=391
 *
 * Half-domain FAFFT kernel (bmul2_half):
 *   Uses bc_1_{SPLIT*64} + btfy_{SPLIT*128} from the Jang FAFFT library.
 *   This kernel processes exactly SPLIT*64-bit inputs and produces
 *   SPLIT*128-bit outputs, which is exactly the size of C0 and C_mid.
 *   Cost ≈ 50% of the corresponding full-domain kernel.
 *
 * Karatsuba formula (in GF(2), subtraction = addition = XOR):
 *   f = f_lo + X * f_hi
 *   g = g_lo + X * g_hi
 *   f*g = C0 + X*(C0 ^ C_mid ^ C2) + X²*C2
 *   where C0 = f_lo*g_lo, C_mid = (f_lo^f_hi)*(g_lo^g_hi), C2 = f_hi*g_hi
 *
 * ═══════════════════════════════════════════════════════════════════════
 * Phase 1c (legacy) — 3-way recursive FAFFT-Karatsuba (HYBRID_KARATSUBA_DEPTH)
 * ═══════════════════════════════════════════════════════════════════════
 * Retained for reference under compile-time flag:
 *   -DUSE_LEGACY_HYBRID=1   activates the old depth-based 3-way scheme
 *   -DHYBRID_KARATSUBA_DEPTH=N   (default 0 = pure FAFFT)
 *
 * Result: depth=0→1.46M, depth=1→4.39M, depth=2→13.16M instret (HQC-128)
 * Shows monotonic degradation: phase1d new 2-way split fixes this.
 *
 * Compile-time flags:
 *   -DKARATSUBA_THRESHOLD=N  base-case for scalar Karatsuba (default 16)
 *   -DUSE_LEGACY_HYBRID=1    use old phase1c 3-way scheme instead
 *   -DHYBRID_KARATSUBA_DEPTH=N (only with USE_LEGACY_HYBRID)
 */

#include <stdint.h>
#include <string.h>
#include "parameters.h"

/* FAFFT full-product and half-domain primitive headers */
#include "../fafft_zbc/ref/gfmul_fft.h"
#include "../fafft_zbc/ref/bitconversion.h"
#include "../fafft_zbc/ref/btfy_ffft.h"
#include "../fafft_zbc/ref/encode.h"
#include "../fafft_zbc/ref/gfv_tower.h"

/* ─── Compile-time parameters ─────────────────────────────────────────── */
#ifndef KARATSUBA_THRESHOLD
#define KARATSUBA_THRESHOLD 16
#endif

#ifndef HYBRID_KARATSUBA_DEPTH
#define HYBRID_KARATSUBA_DEPTH 0
#endif

/* ─── Security-level specific split parameters ────────────────────────── *
 * SPLIT_WORDS : number of 64-bit words in the low half of the split
 * HI_WORDS    : number of 64-bit words in the high half (= VEC_N_SIZE_64 - SPLIT)
 * HALF_IN_U32 : number of uint32_t in one split input (SPLIT_WORDS * 64 / 32)
 * HALF_OUT_U32: number of uint32_t in the product output (SPLIT_WORDS * 128 / 32)
 * bc_half / ibc_half / btfy_half / ibtfy_half: primitives for half domain
 *
 * The half-domain FAFFT processes SPLIT*64-bit inputs (SPLIT words each)
 * using a butterfly of size SPLIT*128 bits.
 */
#if defined(HQC_128)
  #define SPLIT_WORDS         256           /* 2^14 bits / 64 */
  #define HI_WORDS            (VEC_N_SIZE_64 - SPLIT_WORDS)   /* 21 */
  #define HALF_IN_U32         (SPLIT_WORDS * 2)   /* 512  uint32_t */
  #define HALF_OUT_U32        (SPLIT_WORDS * 4)   /* 1024 uint32_t */
  #define HALF_INPUT_BYTES    (SPLIT_WORDS * 8)   /* 2048 bytes    */
  #define HALF_OUTPUT_BYTES   (SPLIT_WORDS * 16)  /* 4096 bytes    */
  #define bc_half(x)          bc_1_16384(x)
  #define ibc_half(x)         ibc_1_32768(x)
  #define btfy_half(x)        btfy_32768(x)
  #define ibtfy_half(x)       ibtfy_32768(x)
#elif defined(HQC_192)
  #define SPLIT_WORDS         512           /* 2^15 bits / 64 */
  #define HI_WORDS            (VEC_N_SIZE_64 - SPLIT_WORDS)   /* 49  */
  #define HALF_IN_U32         (SPLIT_WORDS * 2)   /* 1024 uint32_t */
  #define HALF_OUT_U32        (SPLIT_WORDS * 4)   /* 2048 uint32_t */
  #define HALF_INPUT_BYTES    (SPLIT_WORDS * 8)   /* 4096 bytes    */
  #define HALF_OUTPUT_BYTES   (SPLIT_WORDS * 16)  /* 8192 bytes    */
  #define bc_half(x)          bc_1_32768(x)
  #define ibc_half(x)         ibc_1_65536(x)
  #define btfy_half(x)        btfy_65536(x)
  #define ibtfy_half(x)       ibtfy_65536(x)
#else /* HQC_256 — n=57637, VEC_N_SIZE_64=901; largest 2^k < 901 is 512 */
  #define SPLIT_WORDS         512           /* 2^15 bits / 64 */
  #define HI_WORDS            (VEC_N_SIZE_64 - SPLIT_WORDS)   /* 389 */
  #define HALF_IN_U32         (SPLIT_WORDS * 2)   /* 1024 uint32_t */
  #define HALF_OUT_U32        (SPLIT_WORDS * 4)   /* 2048 uint32_t */
  #define HALF_INPUT_BYTES    (SPLIT_WORDS * 8)   /* 4096 bytes    */
  #define HALF_OUTPUT_BYTES   (SPLIT_WORDS * 16)  /* 8192 bytes    */
  #define bc_half(x)          bc_1_32768(x)
  #define ibc_half(x)         ibc_1_65536(x)
  #define btfy_half(x)        btfy_65536(x)
  #define ibtfy_half(x)       ibtfy_65536(x)
#endif

/* ─── Zbc carry-less multiply base case ──────────────────────────────── */
static __attribute__((always_inline)) inline
void clmul64(uint64_t a, uint64_t b, uint64_t *lo, uint64_t *hi)
{
    __asm__ volatile (
        "clmul  %0, %2, %3\n\t"
        "clmulh %1, %2, %3\n\t"
        : "=&r"(*lo), "=r"(*hi)
        : "r"(a), "r"(b)
    );
}

/* ─── Schoolbook clmul leaf multiply ─────────────────────────────────── */
static void clmul_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < n; j++) {
            uint64_t lo, hi;
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
    }
}

/* ─── Recursive Karatsuba with clmul leaves ──────────────────────────── *
 * Used for the hi×hi residual in the 2-way split.
 */
static void karatsuba_clmul(uint64_t * __restrict__ r,
                             const uint64_t * __restrict__ a,
                             const uint64_t * __restrict__ b,
                             size_t n,
                             uint64_t * __restrict__ tmp)
{
    if (n <= KARATSUBA_THRESHOLD) {
        clmul_leaf(r, a, b, n);
        return;
    }

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0        = tmp;
    uint64_t *z2        = z0 + 2 * n;
    uint64_t *zmid      = z2 + 2 * n;
    uint64_t *ta        = zmid + 2 * n;
    uint64_t *tb        = ta + n;
    uint64_t *child_buf = tmp + 8 * n;

    karatsuba_clmul(z0,   a,     b,     n0, child_buf);
    karatsuba_clmul(z2,   a + m, b + m, n1, child_buf);

    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }
    karatsuba_clmul(zmid, ta, tb, n1, child_buf);

    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i]         ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i] ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
    }
}

/* ─── Half-domain FAFFT kernel ────────────────────────────────────────── *
 *
 * Computes the unreduced product of two SPLIT_WORDS-word polynomials
 * using an FAFFT of half the full domain size.
 *
 * Input:  a_in / b_in — HALF_INPUT_BYTES bytes each (zero-padded if needed)
 * Output: c — HALF_OUTPUT_BYTES bytes  (= 2*SPLIT_WORDS * 8 bytes)
 *
 * Implementation follows the exact pattern of bmul2_4096_to_8192 from
 * Jang et al. (gfmul_fft.c) but using the smaller butterfly/bc primitives.
 *
 * Complexity: ~50% of the corresponding full-domain kernel.
 * No padding waste: SPLIT_WORDS input maps exactly to the half domain.
 */
static void half_prepare(uint32_t *a_out, const uint8_t *a_in)
{
    /* HALF_IN_U32 = SPLIT_WORDS * 2 elements (for bc_half) */
    uint32_t a0[HALF_IN_U32];
    memset(a0, 0, sizeof(a0));
    memcpy(a0, a_in, HALF_INPUT_BYTES);
    bc_half(a0);   /* bit-conversion into evaluation basis */

    /* Transpose: pack 16-element rows into GFT-encoded 32-element blocks.
     * Loop count: HALF_OUT_U32 / 32 iterations. */
    uint32_t temp[16];
    const int loop = HALF_OUT_U32 / 32;
    for (int i = 0; i < loop; i++) {
        for (int j = 0; j < 16; j++)
            temp[j] = a0[loop * j + i];
        encode_trunc_fft(a_out + i * 32, temp);
    }
    btfy_half(a_out);   /* forward butterfly over HALF_OUT_U32 elements */
}

static void half_mul_acc(uint8_t *c,
                         const uint32_t * __restrict__ a_prep,
                         const uint32_t * __restrict__ b_prep)
{
    uint32_t a0[HALF_OUT_U32];
    uint32_t *c32 = (uint32_t *)c;

    /* Pointwise GF(2^32) multiply */
    for (int i = 0; i < HALF_OUT_U32; i += 32)
        gf232v_mul(a0 + i, a_prep + i, b_prep + i);

    ibtfy_half(a0);    /* inverse butterfly */

    /* Transpose back */
    uint32_t temp[32];
    const int loop = HALF_OUT_U32 / 32;
    for (int i = 0; i < HALF_OUT_U32; i += 32) {
        decode_trunc_fft(temp, a0 + i);
        for (int j = 0; j < 32; j++)
            c32[loop * j + i / 32] = temp[j];
    }

    ibc_half(c32);     /* inverse bit-conversion */
}

static void half_kernel(uint8_t *c,
                        const uint8_t *a_in,
                        const uint8_t *b_in)
{
    uint32_t a_u32[HALF_OUT_U32];
    uint32_t b_u32[HALF_OUT_U32];
    half_prepare(a_u32, a_in);
    half_prepare(b_u32, b_in);
    half_mul_acc(c, a_u32, b_u32);
}

/* ─── Modular reduction mod (x^PARAM_N - 1) ─────────────────────────── */
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

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 1d — 2-Way FAFFT-Karatsuba Split
 * ═══════════════════════════════════════════════════════════════════════ */
#ifndef USE_LEGACY_HYBRID

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    /* ── Step 1: C0 = a_lo * b_lo  (half-domain FAFFT) ──────────────── */
    uint8_t C0_bytes[HALF_OUTPUT_BYTES];
    half_kernel(C0_bytes, (const uint8_t *)a, (const uint8_t *)b);

    /* ── Step 2: C2 = a_hi * b_hi  (scalar Karatsuba+clmul) ─────────── */
    /* a_hi starts at a + SPLIT_WORDS, length HI_WORDS */
    uint64_t C2[2 * HI_WORDS];
    /* Karatsuba temp buffer: 16 * HI_WORDS is sufficient for any depth */
    uint64_t kara_tmp[16 * HI_WORDS];
    memset(C2, 0, sizeof(C2));
    karatsuba_clmul(C2, a + SPLIT_WORDS, b + SPLIT_WORDS,
                    HI_WORDS, kara_tmp);

    /* ── Step 3: C_mid = (a_lo ^ a_hi) * (b_lo ^ b_hi)  (half FAFFT) ── */
    /* Sums are at most SPLIT_WORDS long (a_hi pads to zero for i ≥ HI_WORDS) */
    uint64_t sum_a[SPLIT_WORDS], sum_b[SPLIT_WORDS];
    memcpy(sum_a, a, SPLIT_WORDS * sizeof(uint64_t));
    memcpy(sum_b, b, SPLIT_WORDS * sizeof(uint64_t));
    for (size_t i = 0; i < HI_WORDS; i++) {
        sum_a[i] ^= a[SPLIT_WORDS + i];
        sum_b[i] ^= b[SPLIT_WORDS + i];
    }

    uint8_t Cmid_bytes[HALF_OUTPUT_BYTES];
    half_kernel(Cmid_bytes, (const uint8_t *)sum_a, (const uint8_t *)sum_b);

    /* ── Step 4: Assemble unreduced product ──────────────────────────── *
     *   R = C0  +  X*(C0 ^ C_mid ^ C2)  +  X²*C2
     * where X = x^(SPLIT_WORDS*64), i.e., word offset = SPLIT_WORDS.
     *
     * Word layout of the 2*VEC_N_SIZE_64-word result buffer:
     *   offset 0           : C0          (2*SPLIT_WORDS words)
     *   offset SPLIT_WORDS : C0^Cmid^C2  (at most 2*SPLIT_WORDS, capped)
     *   offset 2*SPLIT_WORDS: C2         (2*HI_WORDS words)
     *
     * Any words beyond 2*VEC_N_SIZE_64-1 are zero (guaranteed by the
     * Karatsuba identity; we simply don't write them).
     */
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    memset(unreduced, 0, sizeof(unreduced));

    const uint64_t *C0   = (const uint64_t *)C0_bytes;
    const uint64_t *Cmid = (const uint64_t *)Cmid_bytes;

    /* Add C0 at offset 0 */
    for (size_t i = 0; i < 2 * SPLIT_WORDS; i++)
        unreduced[i] ^= C0[i];

    /* Add C2 at offset 2*SPLIT_WORDS */
    for (size_t i = 0; i < 2 * HI_WORDS; i++)
        unreduced[2 * SPLIT_WORDS + i] ^= C2[i];

    /* Add (C0 ^ Cmid ^ C2) at offset SPLIT_WORDS.
     * Only write indices that land within unreduced[0..2*VEC_N_SIZE_64-1].
     * mid_limit = min(2*SPLIT_WORDS, 2*VEC_N_SIZE_64 - SPLIT_WORDS). */
    size_t mid_limit = 2 * VEC_N_SIZE_64 - SPLIT_WORDS;
    if (mid_limit > 2 * SPLIT_WORDS) mid_limit = 2 * SPLIT_WORDS;

    for (size_t i = 0; i < mid_limit; i++) {
        uint64_t t = C0[i] ^ Cmid[i];
        if (i < 2 * HI_WORDS) t ^= C2[i];
        unreduced[SPLIT_WORDS + i] ^= t;
    }

    /* ── Step 5: Reduce mod x^PARAM_N - 1 ───────────────────────────── */
    reduce(o, unreduced);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Phase 1c legacy — 3-way recursive FAFFT-Karatsuba (reference only)
 * ═══════════════════════════════════════════════════════════════════════ */
#else /* USE_LEGACY_HYBRID */

/* HQC-128 threshold for kernel selection in the legacy path */
#define HQC128_N_WORDS  ((17669 + 63) / 64)   /* = 277 */

static void fafft_leaf(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n)
{
    uint8_t a_buf[VEC_N_SIZE_64 * sizeof(uint64_t)];
    uint8_t b_buf[VEC_N_SIZE_64 * sizeof(uint64_t)];
    memset(a_buf, 0, VEC_N_SIZE_64 * sizeof(uint64_t));
    memset(b_buf, 0, VEC_N_SIZE_64 * sizeof(uint64_t));
    memcpy(a_buf, a, n * sizeof(uint64_t));
    memcpy(b_buf, b, n * sizeof(uint64_t));

    if (n <= HQC128_N_WORDS) {
        uint8_t prod[65536 / 8];
        bmul2_4096_to_8192(prod, a_buf, b_buf);
        memcpy(r, prod, 2 * n * sizeof(uint64_t));
    } else {
        uint8_t prod[131072 / 8];
        bmul2_8192_to_16384(prod, a_buf, b_buf);
        memcpy(r, prod, 2 * n * sizeof(uint64_t));
    }
}

static void legacy_reduce(uint64_t * __restrict__ o,
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

static void hybrid_mul(uint64_t * __restrict__ r,
                       const uint64_t * __restrict__ a,
                       const uint64_t * __restrict__ b,
                       size_t n,
                       int depth,
                       uint64_t * __restrict__ tmp)
{
    if (depth == 0) { fafft_leaf(r, a, b, n); return; }
    if (n <= (size_t)KARATSUBA_THRESHOLD) { clmul_leaf(r, a, b, n); return; }

    size_t m  = n >> 1;
    size_t n0 = m;
    size_t n1 = n - m;

    uint64_t *z0        = tmp;
    uint64_t *z2        = z0   + 2 * n;
    uint64_t *zmid      = z2   + 2 * n;
    uint64_t *ta        = zmid + 2 * n;
    uint64_t *tb        = ta   + n;
    uint64_t *child_buf = tmp  + 8 * n;

    hybrid_mul(z0,   a,     b,     n0, depth - 1, child_buf);
    hybrid_mul(z2,   a + m, b + m, n1, depth - 1, child_buf);
    for (size_t i = 0; i < n1; i++) {
        ta[i] = (i < n0 ? a[i] : 0) ^ a[m + i];
        tb[i] = (i < n0 ? b[i] : 0) ^ b[m + i];
    }
    hybrid_mul(zmid, ta, tb, n1, depth - 1, child_buf);

    memset(r, 0, 2 * n * sizeof(uint64_t));
    for (size_t i = 0; i < 2 * n0; i++) r[i]         ^= z0[i];
    for (size_t i = 0; i < 2 * n1; i++) r[2 * m + i] ^= z2[i];
    for (size_t i = 0; i < 2 * n1; i++) {
        uint64_t z0i = (i < 2 * n0 ? z0[i] : 0);
        r[m + i] ^= zmid[i] ^ z0i ^ z2[i];
    }
}

void vect_mul(uint64_t *o, const uint64_t *a, const uint64_t *b)
{
    uint64_t unreduced[2 * VEC_N_SIZE_64];
    uint64_t tmp_buf[16 * VEC_N_SIZE_64];
    hybrid_mul(unreduced, a, b, VEC_N_SIZE_64, HYBRID_KARATSUBA_DEPTH, tmp_buf);
    legacy_reduce(o, unreduced);
}

#endif /* USE_LEGACY_HYBRID */

/* ═══════════════════════════════════════════════════════════════════════
 * Iter 3 — Key Caching: shared r2 forward transform
 * ═══════════════════════════════════════════════════════════════════════
 * In hqc_pke_encrypt, r2 is multiplied with BOTH h and s:
 *   u   = r2*h  (for ciphertext u)
 *   tmp = r2*s  (for ciphertext v)
 *
 * Each vect_mul call invokes half_prepare(r2_lo) + half_prepare(r2_mid).
 * This function computes both products in one call, sharing the two
 * forward transforms of r2 — saving 2 × half_prepare.
 *
 * Stack budget (HQC-128): ~45 KB peak (well within 512 KB limit).
 */
#ifdef HQC_POLYMUL_HYBRID_KEYCACHE
void vect_mul_r2_keycached(
        uint64_t * __restrict__ u,      /* output: r2 * h (reduced mod x^N-1) */
        uint64_t * __restrict__ v,      /* output: r2 * s (reduced mod x^N-1) */
        const uint64_t *r2,
        const uint64_t *h,
        const uint64_t *s)
{
    /* ── Pre-prepare r2 halves (computed ONCE, reused for both h and s) */
    uint64_t r2_mid[SPLIT_WORDS];
    memcpy(r2_mid, r2, SPLIT_WORDS * sizeof(uint64_t));
    for (size_t i = 0; i < HI_WORDS; i++)
        r2_mid[i] ^= r2[SPLIT_WORDS + i];

    uint32_t r2_lo_prep[HALF_OUT_U32];
    uint32_t r2_mid_prep[HALF_OUT_U32];
    half_prepare(r2_lo_prep, (const uint8_t *)r2);
    half_prepare(r2_mid_prep, (const uint8_t *)r2_mid);

    /* Temporary prepare buffer — reused in each step to reduce peak stack */
    uint32_t tmp_prep[HALF_OUT_U32];

    /* ── Inner helper: assemble Karatsuba and reduce ─────────────────── */
#define KEYCACHE_ASSEMBLE_REDUCE(out, C0b, Cmidb, C2a)                     \
    do {                                                                     \
        uint64_t unreduced[2 * VEC_N_SIZE_64];                              \
        memset(unreduced, 0, sizeof(unreduced));                             \
        const uint64_t *_C0   = (const uint64_t *)(C0b);                   \
        const uint64_t *_Cmid = (const uint64_t *)(Cmidb);                 \
        for (size_t _i = 0; _i < 2 * SPLIT_WORDS; _i++)                    \
            unreduced[_i] ^= _C0[_i];                                       \
        for (size_t _i = 0; _i < 2 * HI_WORDS; _i++)                       \
            unreduced[2 * SPLIT_WORDS + _i] ^= (C2a)[_i];                  \
        size_t _ml = 2 * VEC_N_SIZE_64 - SPLIT_WORDS;                      \
        if (_ml > 2 * SPLIT_WORDS) _ml = 2 * SPLIT_WORDS;                  \
        for (size_t _i = 0; _i < _ml; _i++) {                              \
            uint64_t _t = _C0[_i] ^ _Cmid[_i];                             \
            if (_i < 2 * HI_WORDS) _t ^= (C2a)[_i];                        \
            unreduced[SPLIT_WORDS + _i] ^= _t;                              \
        }                                                                    \
        reduce((out), unreduced);                                            \
    } while (0)

    /* ── Compute u = r2 * h ─────────────────────────────────────────── */
    {
        /* C0_u = r2_lo_prep · h_lo_prep */
        uint8_t C0_bytes[HALF_OUTPUT_BYTES];
        half_prepare(tmp_prep, (const uint8_t *)h);
        half_mul_acc(C0_bytes, r2_lo_prep, tmp_prep);

        /* Cmid_u = r2_mid_prep · h_mid_prep */
        uint8_t Cmid_bytes[HALF_OUTPUT_BYTES];
        uint64_t h_mid[SPLIT_WORDS];
        memcpy(h_mid, h, SPLIT_WORDS * sizeof(uint64_t));
        for (size_t i = 0; i < HI_WORDS; i++) h_mid[i] ^= h[SPLIT_WORDS + i];
        half_prepare(tmp_prep, (const uint8_t *)h_mid);
        half_mul_acc(Cmid_bytes, r2_mid_prep, tmp_prep);

        /* C2_u = r2_hi * h_hi (scalar Karatsuba) */
        uint64_t C2[2 * HI_WORDS];
        uint64_t kara_tmp[16 * HI_WORDS];
        memset(C2, 0, sizeof(C2));
        karatsuba_clmul(C2, r2 + SPLIT_WORDS, h + SPLIT_WORDS, HI_WORDS, kara_tmp);

        KEYCACHE_ASSEMBLE_REDUCE(u, C0_bytes, Cmid_bytes, C2);
    }

    /* ── Compute v = r2 * s (r2 preps reused) ──────────────────────── */
    {
        uint8_t C0_bytes[HALF_OUTPUT_BYTES];
        half_prepare(tmp_prep, (const uint8_t *)s);
        half_mul_acc(C0_bytes, r2_lo_prep, tmp_prep);

        uint8_t Cmid_bytes[HALF_OUTPUT_BYTES];
        uint64_t s_mid[SPLIT_WORDS];
        memcpy(s_mid, s, SPLIT_WORDS * sizeof(uint64_t));
        for (size_t i = 0; i < HI_WORDS; i++) s_mid[i] ^= s[SPLIT_WORDS + i];
        half_prepare(tmp_prep, (const uint8_t *)s_mid);
        half_mul_acc(Cmid_bytes, r2_mid_prep, tmp_prep);

        uint64_t C2[2 * HI_WORDS];
        uint64_t kara_tmp[16 * HI_WORDS];
        memset(C2, 0, sizeof(C2));
        karatsuba_clmul(C2, r2 + SPLIT_WORDS, s + SPLIT_WORDS, HI_WORDS, kara_tmp);

        KEYCACHE_ASSEMBLE_REDUCE(v, C0_bytes, Cmid_bytes, C2);
    }

#undef KEYCACHE_ASSEMBLE_REDUCE
}
#endif /* HQC_POLYMUL_HYBRID_KEYCACHE */
