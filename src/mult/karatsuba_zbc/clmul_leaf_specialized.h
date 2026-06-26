#ifndef HQC_CLMUL_LEAF_SPECIALIZED_H
#define HQC_CLMUL_LEAF_SPECIALIZED_H

/* clmul_leaf_specialized.h
 * Fixed-size fully-unrolled leaf specializations for n=8 and n=9.
 * Uses assign-on-first-write discipline and 4-way ILP grouping.
 * Requires clmul64() to be defined before inclusion.
 *
 * n=14 and n=15 specializations were evaluated but caused +8.4% regression
 * at L256 due to I-cache pressure on the in-order X60 pipeline.  The compact
 * generic loop body at those larger sizes benefits from tight I-cache reuse.
 */

/* ── Fixed-size leaf: n=8 fully unrolled ─────────────────────────────────
 *
 * Assign-on-first-write discipline: no memset required.
 * Row i=0 initializes r[0..8] via direct assigns.
 * Rows i=1..7 XOR into existing positions; the last write r[i+8] is fresh.
 *
 * 4-way ILP: within each row, we issue 4 clmul64 calls before consuming
 * results, hiding the multi-cycle clmul latency on in-order pipelines.
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n8(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo0, hi0, lo1, hi1, lo2, hi2, lo3, hi3;

    /* ── Row i=0: all assigns (first write to r[0..8]) ────────────────── */
    /* 4-way block: j=0..3 */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    clmul64(a[0], b[2], &lo2, &hi2);
    clmul64(a[0], b[3], &lo3, &hi3);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1 ^ lo2;
    r[3] = hi2 ^ lo3;
    r[4] = hi3;

    /* 4-way block: j=4..7 */
    clmul64(a[0], b[4], &lo0, &hi0);
    clmul64(a[0], b[5], &lo1, &hi1);
    clmul64(a[0], b[6], &lo2, &hi2);
    clmul64(a[0], b[7], &lo3, &hi3);
    r[4] ^= lo0;
    r[5]  = hi0 ^ lo1;
    r[6]  = hi1 ^ lo2;
    r[7]  = hi2 ^ lo3;
    r[8]  = hi3;

    /* ── Row i=1 ──────────────────────────────────────────────────────── */
    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    clmul64(a[1], b[2], &lo2, &hi2);
    clmul64(a[1], b[3], &lo3, &hi3);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;
    r[3] ^= hi1 ^ lo2;
    r[4] ^= hi2 ^ lo3;
    r[5] ^= hi3;

    clmul64(a[1], b[4], &lo0, &hi0);
    clmul64(a[1], b[5], &lo1, &hi1);
    clmul64(a[1], b[6], &lo2, &hi2);
    clmul64(a[1], b[7], &lo3, &hi3);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1 ^ lo2;
    r[8] ^= hi2 ^ lo3;
    r[9]  = hi3;   /* first write */

    /* ── Row i=2 ──────────────────────────────────────────────────────── */
    clmul64(a[2], b[0], &lo0, &hi0);
    clmul64(a[2], b[1], &lo1, &hi1);
    clmul64(a[2], b[2], &lo2, &hi2);
    clmul64(a[2], b[3], &lo3, &hi3);
    r[2] ^= lo0;
    r[3] ^= hi0 ^ lo1;
    r[4] ^= hi1 ^ lo2;
    r[5] ^= hi2 ^ lo3;
    r[6] ^= hi3;

    clmul64(a[2], b[4], &lo0, &hi0);
    clmul64(a[2], b[5], &lo1, &hi1);
    clmul64(a[2], b[6], &lo2, &hi2);
    clmul64(a[2], b[7], &lo3, &hi3);
    r[6]  ^= lo0;
    r[7]  ^= hi0 ^ lo1;
    r[8]  ^= hi1 ^ lo2;
    r[9]  ^= hi2 ^ lo3;
    r[10]  = hi3;  /* first write */

    /* ── Row i=3 ──────────────────────────────────────────────────────── */
    clmul64(a[3], b[0], &lo0, &hi0);
    clmul64(a[3], b[1], &lo1, &hi1);
    clmul64(a[3], b[2], &lo2, &hi2);
    clmul64(a[3], b[3], &lo3, &hi3);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1 ^ lo2;
    r[6] ^= hi2 ^ lo3;
    r[7] ^= hi3;

    clmul64(a[3], b[4], &lo0, &hi0);
    clmul64(a[3], b[5], &lo1, &hi1);
    clmul64(a[3], b[6], &lo2, &hi2);
    clmul64(a[3], b[7], &lo3, &hi3);
    r[7]  ^= lo0;
    r[8]  ^= hi0 ^ lo1;
    r[9]  ^= hi1 ^ lo2;
    r[10] ^= hi2 ^ lo3;
    r[11]  = hi3;  /* first write */

    /* ── Row i=4 ──────────────────────────────────────────────────────── */
    clmul64(a[4], b[0], &lo0, &hi0);
    clmul64(a[4], b[1], &lo1, &hi1);
    clmul64(a[4], b[2], &lo2, &hi2);
    clmul64(a[4], b[3], &lo3, &hi3);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1 ^ lo2;
    r[7] ^= hi2 ^ lo3;
    r[8] ^= hi3;

    clmul64(a[4], b[4], &lo0, &hi0);
    clmul64(a[4], b[5], &lo1, &hi1);
    clmul64(a[4], b[6], &lo2, &hi2);
    clmul64(a[4], b[7], &lo3, &hi3);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1 ^ lo2;
    r[11] ^= hi2 ^ lo3;
    r[12]  = hi3;  /* first write */

    /* ── Row i=5 ──────────────────────────────────────────────────────── */
    clmul64(a[5], b[0], &lo0, &hi0);
    clmul64(a[5], b[1], &lo1, &hi1);
    clmul64(a[5], b[2], &lo2, &hi2);
    clmul64(a[5], b[3], &lo3, &hi3);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1 ^ lo2;
    r[8] ^= hi2 ^ lo3;
    r[9] ^= hi3;

    clmul64(a[5], b[4], &lo0, &hi0);
    clmul64(a[5], b[5], &lo1, &hi1);
    clmul64(a[5], b[6], &lo2, &hi2);
    clmul64(a[5], b[7], &lo3, &hi3);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1 ^ lo2;
    r[12] ^= hi2 ^ lo3;
    r[13]  = hi3;  /* first write */

    /* ── Row i=6 ──────────────────────────────────────────────────────── */
    clmul64(a[6], b[0], &lo0, &hi0);
    clmul64(a[6], b[1], &lo1, &hi1);
    clmul64(a[6], b[2], &lo2, &hi2);
    clmul64(a[6], b[3], &lo3, &hi3);
    r[6] ^= lo0;
    r[7] ^= hi0 ^ lo1;
    r[8] ^= hi1 ^ lo2;
    r[9] ^= hi2 ^ lo3;
    r[10] ^= hi3;

    clmul64(a[6], b[4], &lo0, &hi0);
    clmul64(a[6], b[5], &lo1, &hi1);
    clmul64(a[6], b[6], &lo2, &hi2);
    clmul64(a[6], b[7], &lo3, &hi3);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1 ^ lo2;
    r[13] ^= hi2 ^ lo3;
    r[14]  = hi3;  /* first write */

    /* ── Row i=7 ──────────────────────────────────────────────────────── */
    clmul64(a[7], b[0], &lo0, &hi0);
    clmul64(a[7], b[1], &lo1, &hi1);
    clmul64(a[7], b[2], &lo2, &hi2);
    clmul64(a[7], b[3], &lo3, &hi3);
    r[7]  ^= lo0;
    r[8]  ^= hi0 ^ lo1;
    r[9]  ^= hi1 ^ lo2;
    r[10] ^= hi2 ^ lo3;
    r[11] ^= hi3;

    clmul64(a[7], b[4], &lo0, &hi0);
    clmul64(a[7], b[5], &lo1, &hi1);
    clmul64(a[7], b[6], &lo2, &hi2);
    clmul64(a[7], b[7], &lo3, &hi3);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1 ^ lo2;
    r[14] ^= hi2 ^ lo3;
    r[15]  = hi3;  /* first write */
}

/* ── Fixed-size leaf: n=9 fully unrolled ─────────────────────────────────
 *
 * Same assign-on-first-write discipline extended to n=9.
 * Row i=0 initializes r[0..9] via direct assigns.
 * Rows i=1..8 XOR into existing positions; the last write r[i+9] is fresh.
 *
 * 4-way ILP: issue 4 clmul64 in a batch, then consume all 4 results.
 * The odd column (j=8) handled as a scalar tail each row.
 * Output: r[0..17] (2*9 = 18 words).
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n9(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo0, hi0, lo1, hi1, lo2, hi2, lo3, hi3;

    /* ── Row i=0: all assigns (first write to r[0..9]) ────────────────── */
    /* 4-way block: j=0..3 */
    clmul64(a[0], b[0], &lo0, &hi0);
    clmul64(a[0], b[1], &lo1, &hi1);
    clmul64(a[0], b[2], &lo2, &hi2);
    clmul64(a[0], b[3], &lo3, &hi3);
    r[0] = lo0;
    r[1] = hi0 ^ lo1;
    r[2] = hi1 ^ lo2;
    r[3] = hi2 ^ lo3;
    r[4] = hi3;

    /* 4-way block: j=4..7 */
    clmul64(a[0], b[4], &lo0, &hi0);
    clmul64(a[0], b[5], &lo1, &hi1);
    clmul64(a[0], b[6], &lo2, &hi2);
    clmul64(a[0], b[7], &lo3, &hi3);
    r[4] ^= lo0;
    r[5]  = hi0 ^ lo1;
    r[6]  = hi1 ^ lo2;
    r[7]  = hi2 ^ lo3;
    r[8]  = hi3;

    /* Scalar tail: j=8 */
    clmul64(a[0], b[8], &lo0, &hi0);
    r[8] ^= lo0;
    r[9]  = hi0;

    /* ── Row i=1 ──────────────────────────────────────────────────────── */
    clmul64(a[1], b[0], &lo0, &hi0);
    clmul64(a[1], b[1], &lo1, &hi1);
    clmul64(a[1], b[2], &lo2, &hi2);
    clmul64(a[1], b[3], &lo3, &hi3);
    r[1] ^= lo0;
    r[2] ^= hi0 ^ lo1;
    r[3] ^= hi1 ^ lo2;
    r[4] ^= hi2 ^ lo3;
    r[5] ^= hi3;

    clmul64(a[1], b[4], &lo0, &hi0);
    clmul64(a[1], b[5], &lo1, &hi1);
    clmul64(a[1], b[6], &lo2, &hi2);
    clmul64(a[1], b[7], &lo3, &hi3);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1 ^ lo2;
    r[8] ^= hi2 ^ lo3;
    r[9] ^= hi3;

    /* Scalar tail: j=8 */
    clmul64(a[1], b[8], &lo0, &hi0);
    r[9]  ^= lo0;
    r[10]  = hi0;  /* first write */

    /* ── Row i=2 ──────────────────────────────────────────────────────── */
    clmul64(a[2], b[0], &lo0, &hi0);
    clmul64(a[2], b[1], &lo1, &hi1);
    clmul64(a[2], b[2], &lo2, &hi2);
    clmul64(a[2], b[3], &lo3, &hi3);
    r[2] ^= lo0;
    r[3] ^= hi0 ^ lo1;
    r[4] ^= hi1 ^ lo2;
    r[5] ^= hi2 ^ lo3;
    r[6] ^= hi3;

    clmul64(a[2], b[4], &lo0, &hi0);
    clmul64(a[2], b[5], &lo1, &hi1);
    clmul64(a[2], b[6], &lo2, &hi2);
    clmul64(a[2], b[7], &lo3, &hi3);
    r[6]  ^= lo0;
    r[7]  ^= hi0 ^ lo1;
    r[8]  ^= hi1 ^ lo2;
    r[9]  ^= hi2 ^ lo3;
    r[10] ^= hi3;

    clmul64(a[2], b[8], &lo0, &hi0);
    r[10] ^= lo0;
    r[11]  = hi0;  /* first write */

    /* ── Row i=3 ──────────────────────────────────────────────────────── */
    clmul64(a[3], b[0], &lo0, &hi0);
    clmul64(a[3], b[1], &lo1, &hi1);
    clmul64(a[3], b[2], &lo2, &hi2);
    clmul64(a[3], b[3], &lo3, &hi3);
    r[3] ^= lo0;
    r[4] ^= hi0 ^ lo1;
    r[5] ^= hi1 ^ lo2;
    r[6] ^= hi2 ^ lo3;
    r[7] ^= hi3;

    clmul64(a[3], b[4], &lo0, &hi0);
    clmul64(a[3], b[5], &lo1, &hi1);
    clmul64(a[3], b[6], &lo2, &hi2);
    clmul64(a[3], b[7], &lo3, &hi3);
    r[7]  ^= lo0;
    r[8]  ^= hi0 ^ lo1;
    r[9]  ^= hi1 ^ lo2;
    r[10] ^= hi2 ^ lo3;
    r[11] ^= hi3;

    clmul64(a[3], b[8], &lo0, &hi0);
    r[11] ^= lo0;
    r[12]  = hi0;  /* first write */

    /* ── Row i=4 ──────────────────────────────────────────────────────── */
    clmul64(a[4], b[0], &lo0, &hi0);
    clmul64(a[4], b[1], &lo1, &hi1);
    clmul64(a[4], b[2], &lo2, &hi2);
    clmul64(a[4], b[3], &lo3, &hi3);
    r[4] ^= lo0;
    r[5] ^= hi0 ^ lo1;
    r[6] ^= hi1 ^ lo2;
    r[7] ^= hi2 ^ lo3;
    r[8] ^= hi3;

    clmul64(a[4], b[4], &lo0, &hi0);
    clmul64(a[4], b[5], &lo1, &hi1);
    clmul64(a[4], b[6], &lo2, &hi2);
    clmul64(a[4], b[7], &lo3, &hi3);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1 ^ lo2;
    r[11] ^= hi2 ^ lo3;
    r[12] ^= hi3;

    clmul64(a[4], b[8], &lo0, &hi0);
    r[12] ^= lo0;
    r[13]  = hi0;  /* first write */

    /* ── Row i=5 ──────────────────────────────────────────────────────── */
    clmul64(a[5], b[0], &lo0, &hi0);
    clmul64(a[5], b[1], &lo1, &hi1);
    clmul64(a[5], b[2], &lo2, &hi2);
    clmul64(a[5], b[3], &lo3, &hi3);
    r[5] ^= lo0;
    r[6] ^= hi0 ^ lo1;
    r[7] ^= hi1 ^ lo2;
    r[8] ^= hi2 ^ lo3;
    r[9] ^= hi3;

    clmul64(a[5], b[4], &lo0, &hi0);
    clmul64(a[5], b[5], &lo1, &hi1);
    clmul64(a[5], b[6], &lo2, &hi2);
    clmul64(a[5], b[7], &lo3, &hi3);
    r[9]  ^= lo0;
    r[10] ^= hi0 ^ lo1;
    r[11] ^= hi1 ^ lo2;
    r[12] ^= hi2 ^ lo3;
    r[13] ^= hi3;

    clmul64(a[5], b[8], &lo0, &hi0);
    r[13] ^= lo0;
    r[14]  = hi0;  /* first write */

    /* ── Row i=6 ──────────────────────────────────────────────────────── */
    clmul64(a[6], b[0], &lo0, &hi0);
    clmul64(a[6], b[1], &lo1, &hi1);
    clmul64(a[6], b[2], &lo2, &hi2);
    clmul64(a[6], b[3], &lo3, &hi3);
    r[6]  ^= lo0;
    r[7]  ^= hi0 ^ lo1;
    r[8]  ^= hi1 ^ lo2;
    r[9]  ^= hi2 ^ lo3;
    r[10] ^= hi3;

    clmul64(a[6], b[4], &lo0, &hi0);
    clmul64(a[6], b[5], &lo1, &hi1);
    clmul64(a[6], b[6], &lo2, &hi2);
    clmul64(a[6], b[7], &lo3, &hi3);
    r[10] ^= lo0;
    r[11] ^= hi0 ^ lo1;
    r[12] ^= hi1 ^ lo2;
    r[13] ^= hi2 ^ lo3;
    r[14] ^= hi3;

    clmul64(a[6], b[8], &lo0, &hi0);
    r[14] ^= lo0;
    r[15]  = hi0;  /* first write */

    /* ── Row i=7 ──────────────────────────────────────────────────────── */
    clmul64(a[7], b[0], &lo0, &hi0);
    clmul64(a[7], b[1], &lo1, &hi1);
    clmul64(a[7], b[2], &lo2, &hi2);
    clmul64(a[7], b[3], &lo3, &hi3);
    r[7]  ^= lo0;
    r[8]  ^= hi0 ^ lo1;
    r[9]  ^= hi1 ^ lo2;
    r[10] ^= hi2 ^ lo3;
    r[11] ^= hi3;

    clmul64(a[7], b[4], &lo0, &hi0);
    clmul64(a[7], b[5], &lo1, &hi1);
    clmul64(a[7], b[6], &lo2, &hi2);
    clmul64(a[7], b[7], &lo3, &hi3);
    r[11] ^= lo0;
    r[12] ^= hi0 ^ lo1;
    r[13] ^= hi1 ^ lo2;
    r[14] ^= hi2 ^ lo3;
    r[15] ^= hi3;

    clmul64(a[7], b[8], &lo0, &hi0);
    r[15] ^= lo0;
    r[16]  = hi0;  /* first write */

    /* ── Row i=8 ──────────────────────────────────────────────────────── */
    clmul64(a[8], b[0], &lo0, &hi0);
    clmul64(a[8], b[1], &lo1, &hi1);
    clmul64(a[8], b[2], &lo2, &hi2);
    clmul64(a[8], b[3], &lo3, &hi3);
    r[8]  ^= lo0;
    r[9]  ^= hi0 ^ lo1;
    r[10] ^= hi1 ^ lo2;
    r[11] ^= hi2 ^ lo3;
    r[12] ^= hi3;

    clmul64(a[8], b[4], &lo0, &hi0);
    clmul64(a[8], b[5], &lo1, &hi1);
    clmul64(a[8], b[6], &lo2, &hi2);
    clmul64(a[8], b[7], &lo3, &hi3);
    r[12] ^= lo0;
    r[13] ^= hi0 ^ lo1;
    r[14] ^= hi1 ^ lo2;
    r[15] ^= hi2 ^ lo3;
    r[16] ^= hi3;

    clmul64(a[8], b[8], &lo0, &hi0);
    r[16] ^= lo0;
    r[17]  = hi0;  /* first write */
}

#endif /* HQC_CLMUL_LEAF_SPECIALIZED_H */
