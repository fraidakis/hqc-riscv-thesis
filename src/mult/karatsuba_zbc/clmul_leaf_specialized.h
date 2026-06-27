#ifndef HQC_CLMUL_LEAF_SPECIALIZED_H
#define HQC_CLMUL_LEAF_SPECIALIZED_H

/* clmul_leaf_specialized.h
 * Fixed-size fully-unrolled leaf specializations for n=8 and n=9.
 * Uses assign-on-first-write discipline and 2-way pair scheduling.
 * Requires clmul64() to be defined before inclusion.
 *
 * Scheduling strategy: issue 2 clmul64 calls (one pair) before consuming
 * results. Hardware measurement on SpacemiT K1 X60 (BPI-F3) showed that
 * 4-way ILP grouping provides no measurable benefit over 2-way at L128/L256
 * and degrades performance in the vector_rvv variant (+1.6%) due to increased
 * I-cache pressure. The simpler 2-way structure is retained for both variants.
 *
 * n=14 and n=15 specializations were evaluated but caused +8.4% regression
 * at L256 due to I-cache pressure on the in-order X60 pipeline. The compact
 * generic loop body at those larger sizes benefits from tight I-cache reuse.
 */

/* ── Fixed-size leaf: n=9 fully unrolled (81 clmul64, zero branches) ─────
 *
 * Same zero-free write discipline extended to n=9 (odd).
 *
 * Structure mirrors clmul_leaf with n=9 unrolled:
 *   Row i=0: init pair(j=0,1) + loop pairs(j=2,3),(j=4,5),(j=6,7) + tail(j=8)
 *            → assigns r[0..9]
 *   Row i≥1: main pairs(j=0..6) + odd-tail single(j=8)
 *            → XOR into r[i..i+8]; fresh assign to r[i+n]=r[i+9]
 *
 * First-write proof sketch:
 *   r[0..9]  : first-written by row 0
 *   r[k] for k=10..17 : first-written by row k-9 odd-tail
 *   r[i+0..i+8] for i≥1 : all written by prior rows ✓
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n9(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo0, hi0, lo1, hi1;

    /* ── Row i=0 ──────────────────────────────────────────────────────── */

    /* Init pair (j=0, j=1) */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1;

    /* Loop pair (j=2, j=3) */
    clmul64(a[0], b[2], &lo0, &hi0);
    clmul64(a[0], b[3], &lo1, &hi1);
    r[2] ^= lo0;
    r[3]  = hi0 ^ lo1;
    r[4]  = hi1;

    /* Loop pair (j=4, j=5) */
    clmul64(a[0], b[4], &lo0, &hi0);
    clmul64(a[0], b[5], &lo1, &hi1);
    r[4] ^= lo0;
    r[5]  = hi0 ^ lo1;
    r[6]  = hi1;

    /* Loop pair (j=6, j=7) */
    clmul64(a[0], b[6], &lo0, &hi0);
    clmul64(a[0], b[7], &lo1, &hi1);
    r[6] ^= lo0;
    r[7]  = hi0 ^ lo1;
    r[8]  = hi1;

    /* Odd tail (j=8): single element */
    clmul64(a[0], b[8], &lo0, &hi0);
    r[8] ^= lo0;
    r[9]  = hi0;

    /* ── Row i=1 ──────────────────────────────────────────────────────── */

    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;
    r[3] ^= hi1;

    clmul64(a[1], b[2], &lo0, &hi0);
    clmul64(a[1], b[3], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    clmul64(a[1], b[4], &lo0, &hi0);
    clmul64(a[1], b[5], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[1], b[6], &lo0, &hi0);
    clmul64(a[1], b[7], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    /* Odd-tail (j=8): r[1+9]=r[10] is fresh assign */
    clmul64(a[1], b[8], &lo0, &hi0);
    r[9]  ^= lo0;
    r[10]  = hi0;

    /* ── Row i=2 ──────────────────────────────────────────────────────── */

    clmul64(a[2], b[0], &lo0, &hi0);
    clmul64(a[2], b[1], &lo1, &hi1);
    r[2] ^= lo0;
    r[3] ^= hi0 ^ lo1;
    r[4] ^= hi1;

    clmul64(a[2], b[2], &lo0, &hi0);
    clmul64(a[2], b[3], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1;

    clmul64(a[2], b[4], &lo0, &hi0);
    clmul64(a[2], b[5], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[2], b[6], &lo0, &hi0);
    clmul64(a[2], b[7], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    /* Odd-tail (j=8): r[2+9]=r[11] is fresh assign */
    clmul64(a[2], b[8], &lo0, &hi0);
    r[10] ^= lo0;
    r[11]  = hi0;

    /* ── Row i=3 ──────────────────────────────────────────────────────── */

    clmul64(a[3], b[0], &lo0, &hi0);
    clmul64(a[3], b[1], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    clmul64(a[3], b[2], &lo0, &hi0);
    clmul64(a[3], b[3], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[3], b[4], &lo0, &hi0);
    clmul64(a[3], b[5], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[3], b[6], &lo0, &hi0);
    clmul64(a[3], b[7], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    /* Odd-tail (j=8): r[3+9]=r[12] is fresh assign */
    clmul64(a[3], b[8], &lo0, &hi0);
    r[11] ^= lo0;
    r[12]  = hi0;

    /* ── Row i=4 ──────────────────────────────────────────────────────── */

    clmul64(a[4], b[0], &lo0, &hi0);
    clmul64(a[4], b[1], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1;

    clmul64(a[4], b[2], &lo0, &hi0);
    clmul64(a[4], b[3], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[4], b[4], &lo0, &hi0);
    clmul64(a[4], b[5], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    clmul64(a[4], b[6], &lo0, &hi0);
    clmul64(a[4], b[7], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    /* Odd-tail (j=8): r[4+9]=r[13] is fresh assign */
    clmul64(a[4], b[8], &lo0, &hi0);
    r[12] ^= lo0;
    r[13]  = hi0;

    /* ── Row i=5 ──────────────────────────────────────────────────────── */

    clmul64(a[5], b[0], &lo0, &hi0);
    clmul64(a[5], b[1], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[5], b[2], &lo0, &hi0);
    clmul64(a[5], b[3], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[5], b[4], &lo0, &hi0);
    clmul64(a[5], b[5], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    clmul64(a[5], b[6], &lo0, &hi0);
    clmul64(a[5], b[7], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1;

    /* Odd-tail (j=8): r[5+9]=r[14] is fresh assign */
    clmul64(a[5], b[8], &lo0, &hi0);
    r[13] ^= lo0;
    r[14]  = hi0;

    /* ── Row i=6 ──────────────────────────────────────────────────────── */

    clmul64(a[6], b[0], &lo0, &hi0);
    clmul64(a[6], b[1], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[6], b[2], &lo0, &hi0);
    clmul64(a[6], b[3], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    clmul64(a[6], b[4], &lo0, &hi0);
    clmul64(a[6], b[5], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    clmul64(a[6], b[6], &lo0, &hi0);
    clmul64(a[6], b[7], &lo1, &hi1);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14] ^= hi1;

    /* Odd-tail (j=8): r[6+9]=r[15] is fresh assign */
    clmul64(a[6], b[8], &lo0, &hi0);
    r[14] ^= lo0;
    r[15]  = hi0;

    /* ── Row i=7 ──────────────────────────────────────────────────────── */

    clmul64(a[7], b[0], &lo0, &hi0);
    clmul64(a[7], b[1], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[7], b[2], &lo0, &hi0);
    clmul64(a[7], b[3], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    clmul64(a[7], b[4], &lo0, &hi0);
    clmul64(a[7], b[5], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1;

    clmul64(a[7], b[6], &lo0, &hi0);
    clmul64(a[7], b[7], &lo1, &hi1);
    r[13] ^= lo0;
    r[14] ^= hi0 ^ lo1;
    r[15] ^= hi1;

    /* Odd-tail (j=8): r[7+9]=r[16] is fresh assign */
    clmul64(a[7], b[8], &lo0, &hi0);
    r[15] ^= lo0;
    r[16]  = hi0;

    /* ── Row i=8 ──────────────────────────────────────────────────────── */

    clmul64(a[8], b[0], &lo0, &hi0);
    clmul64(a[8], b[1], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    clmul64(a[8], b[2], &lo0, &hi0);
    clmul64(a[8], b[3], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    clmul64(a[8], b[4], &lo0, &hi0);
    clmul64(a[8], b[5], &lo1, &hi1);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14] ^= hi1;

    clmul64(a[8], b[6], &lo0, &hi0);
    clmul64(a[8], b[7], &lo1, &hi1);
    r[14] ^= lo0;
    r[15] ^= hi0 ^ lo1;
    r[16] ^= hi1;

    /* Odd-tail (j=8): r[8+9]=r[17] is fresh assign */
    clmul64(a[8], b[8], &lo0, &hi0);
    r[16] ^= lo0;
    r[17]  = hi0;
}

/* ── Fixed-size leaf: n=8 fully unrolled (64 clmul64, zero branches) ─────
 *
 * Same zero-free write discipline as clmul_leaf_n4 extended to n=8.
 *
 * Structure mirrors clmul_leaf with n=8 unrolled:
 *   Row i=0: init pair(j=0,1) + loop pairs(j=2,3),(j=4,5),(j=6,7)
 *            → assigns r[0..8]
 *   Row i≥1: main pairs(j=0,1),(j=2,3),(j=4,5) + even-tail pair(j=6,7)
 *            → XOR into r[i..i+7]; fresh assign to r[i+n]=r[i+8]
 *
 * First-write proof sketch:
 *   r[0..8]  : first-written by row 0 (row 0 init/loop pairs)
 *   r[9]     : first-written by row 1 even-tail (r[1+8]=r[9])
 *   r[k]     : first-written by row k-8 even-tail for k = 9..15
 *   r[i+0..i+7] for i≥1 : all have been written by row 0 or prior rows ✓
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n8(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo0, hi0, lo1, hi1;

    /* ── Row i=0 ──────────────────────────────────────────────────────── */

    /* Init pair (j=0, j=1) */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1;

    /* Loop pair (j=2, j=3) */
    clmul64(a[0], b[2], &lo0, &hi0);
    clmul64(a[0], b[3], &lo1, &hi1);
    r[2] ^= lo0;
    r[3]  = hi0 ^ lo1;
    r[4]  = hi1;

    /* Loop pair (j=4, j=5) */
    clmul64(a[0], b[4], &lo0, &hi0);
    clmul64(a[0], b[5], &lo1, &hi1);
    r[4] ^= lo0;
    r[5]  = hi0 ^ lo1;
    r[6]  = hi1;

    /* Loop pair (j=6, j=7) */
    clmul64(a[0], b[6], &lo0, &hi0);
    clmul64(a[0], b[7], &lo1, &hi1);
    r[6] ^= lo0;
    r[7]  = hi0 ^ lo1;
    r[8]  = hi1;          /* r[0+n]=r[8]: fresh assign (ends row 0) */

    /* ── Row i=1 ──────────────────────────────────────────────────────── */

    /* Main loop pair (j=0, j=1): j+1=1 < n-1=7 → all three outputs XOR */
    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;
    r[3] ^= hi1;

    /* Main loop pair (j=2, j=3): j+1=3 < 7 */
    clmul64(a[1], b[2], &lo0, &hi0);
    clmul64(a[1], b[3], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    /* Main loop pair (j=4, j=5): j+1=5 < 7 */
    clmul64(a[1], b[4], &lo0, &hi0);
    clmul64(a[1], b[5], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[1+8]=r[9] is fresh assign */
    clmul64(a[1], b[6], &lo0, &hi0);
    clmul64(a[1], b[7], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9]  = hi1;

    /* ── Row i=2 ──────────────────────────────────────────────────────── */

    clmul64(a[2], b[0], &lo0, &hi0);
    clmul64(a[2], b[1], &lo1, &hi1);
    r[2] ^= lo0;
    r[3] ^= hi0 ^ lo1;
    r[4] ^= hi1;

    clmul64(a[2], b[2], &lo0, &hi0);
    clmul64(a[2], b[3], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1;

    clmul64(a[2], b[4], &lo0, &hi0);
    clmul64(a[2], b[5], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[2+8]=r[10] is fresh assign */
    clmul64(a[2], b[6], &lo0, &hi0);
    clmul64(a[2], b[7], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10]  = hi1;

    /* ── Row i=3 ──────────────────────────────────────────────────────── */

    clmul64(a[3], b[0], &lo0, &hi0);
    clmul64(a[3], b[1], &lo1, &hi1);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1;

    clmul64(a[3], b[2], &lo0, &hi0);
    clmul64(a[3], b[3], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[3], b[4], &lo0, &hi0);
    clmul64(a[3], b[5], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[3+8]=r[11] is fresh assign */
    clmul64(a[3], b[6], &lo0, &hi0);
    clmul64(a[3], b[7], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11]  = hi1;

    /* ── Row i=4 ──────────────────────────────────────────────────────── */

    clmul64(a[4], b[0], &lo0, &hi0);
    clmul64(a[4], b[1], &lo1, &hi1);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1;

    clmul64(a[4], b[2], &lo0, &hi0);
    clmul64(a[4], b[3], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[4], b[4], &lo0, &hi0);
    clmul64(a[4], b[5], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[4+8]=r[12] is fresh assign */
    clmul64(a[4], b[6], &lo0, &hi0);
    clmul64(a[4], b[7], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12]  = hi1;

    /* ── Row i=5 ──────────────────────────────────────────────────────── */

    clmul64(a[5], b[0], &lo0, &hi0);
    clmul64(a[5], b[1], &lo1, &hi1);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1;

    clmul64(a[5], b[2], &lo0, &hi0);
    clmul64(a[5], b[3], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[5], b[4], &lo0, &hi0);
    clmul64(a[5], b[5], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[5+8]=r[13] is fresh assign */
    clmul64(a[5], b[6], &lo0, &hi0);
    clmul64(a[5], b[7], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13]  = hi1;

    /* ── Row i=6 ──────────────────────────────────────────────────────── */

    clmul64(a[6], b[0], &lo0, &hi0);
    clmul64(a[6], b[1], &lo1, &hi1);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1;

    clmul64(a[6], b[2], &lo0, &hi0);
    clmul64(a[6], b[3], &lo1, &hi1);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1;

    clmul64(a[6], b[4], &lo0, &hi0);
    clmul64(a[6], b[5], &lo1, &hi1);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[6+8]=r[14] is fresh assign */
    clmul64(a[6], b[6], &lo0, &hi0);
    clmul64(a[6], b[7], &lo1, &hi1);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14]  = hi1;

    /* ── Row i=7 ──────────────────────────────────────────────────────── */

    clmul64(a[7], b[0], &lo0, &hi0);
    clmul64(a[7], b[1], &lo1, &hi1);
    r[7] ^= lo0;
    r[8] ^= hi0 ^ lo1;
    r[9] ^= hi1;

    clmul64(a[7], b[2], &lo0, &hi0);
    clmul64(a[7], b[3], &lo1, &hi1);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1;

    clmul64(a[7], b[4], &lo0, &hi0);
    clmul64(a[7], b[5], &lo1, &hi1);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1;

    /* Even-tail pair (j=6, j=7): r[7+8]=r[15] is fresh assign */
    clmul64(a[7], b[6], &lo0, &hi0);
    clmul64(a[7], b[7], &lo1, &hi1);
    r[13] ^= lo0;
    r[14] ^= hi0 ^ lo1;
    r[15]  = hi1;
}

#endif /* HQC_CLMUL_LEAF_SPECIALIZED_H */
