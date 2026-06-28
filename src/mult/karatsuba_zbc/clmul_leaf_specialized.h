#ifndef HQC_CLMUL_LEAF_SPECIALIZED_H
#define HQC_CLMUL_LEAF_SPECIALIZED_H

/* clmul_leaf_specialized.h
 * Hybrid leaf specializations for n=8, n=9, n=14, and n=15.
 * Uses assign-on-first-write discipline — eliminates memset.
 * All rows use compact inner loops for I-cache efficiency and consistency.
 * Row 0: first clmul64 assigns r[0]=lo, r[1]=hi; remaining j=1..n-1 via loop.
 * Rows 1..n-2: XOR-accumulate inner loop + fresh assign on last column.
 * Row n-1: all positions already initialized — pure XOR-accumulate loop.
 * Requires clmul64() to be defined before inclusion.
 *
 * Each clmul64(a[i], b[j]) produces lo → r[i+j] and hi → r[i+j+1].
 *
 * Coverage:
 *   n=8, n=9  — HQC-128 and HQC-192 leaf sizes (T=16 threshold)
 *   n=14, n=15 — HQC-256 leaf sizes (T=16 threshold)
 *
 * Design rationale:
 *   The compact inner loop body (~6 instructions: clmul, clmulh, 2×xor,
 *   2×store) fits in 1–2 I-cache lines and stays hot across all iterations.
 *   Benchmarking on the SpacemiT X60 shows the compact-loop row-0 is
 *   performance-neutral vs full unrolling for n=8/9 (within ±0.6% noise),
 *   so we use the uniform compact style across all leaf sizes for
 *   consistency and reduced code maintenance.
 *   Specialized n=14/n=15 leaves yield an additional −8.7% at L256 vs memset.
 */

/* ── Fixed-size leaf: n=9 (compact loops, assign-on-first-write) ─────────
 *
 * Assign-on-first-write discipline:
 *   Row i=0: r[0..9] first-written via compact loop
 *   Row i=1..7: XOR-accumulates into r[i..i+8]; fresh assign to r[i+9]
 *   Row i=8: all positions already initialized — pure XOR-accumulate
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n9(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo, hi;

    /* ── Row i=0: assign-on-first-write (initializes r[0..9]) ─────────── */
    clmul64(a[0], b[0], &lo, &hi);
    r[0] = lo;
    r[1] = hi;

    for (size_t j = 1; j < 9; j++) {
        clmul64(a[0], b[j], &lo, &hi);
        r[j]     ^= lo;
        r[j + 1]  = hi;
    }

    /* ── Rows i=1..7: XOR-accumulate with compact inner loop ─────────── */
    for (size_t i = 1; i < 8; i++) {
        for (size_t j = 0; j < 8; j++) {
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
        /* Last column j=8: r[i+9] is a fresh assign */
        clmul64(a[i], b[8], &lo, &hi);
        r[i + 8] ^= lo;
        r[i + 9]  = hi;
    }

    /* ── Row i=8: all positions already initialized ──────────────────── */
    for (size_t j = 0; j < 8; j++) {
        clmul64(a[8], b[j], &lo, &hi);
        r[8 + j]     ^= lo;
        r[8 + j + 1] ^= hi;
    }
    /* Last column j=8 */
    clmul64(a[8], b[8], &lo, &hi);
    r[16] ^= lo;
    r[17]  = hi;
}


/* ── Fixed-size leaf: n=8 (compact loops, assign-on-first-write) ─────────
 *
 * Assign-on-first-write discipline:
 *   Row i=0: r[0..8] first-written via compact loop
 *   Row i=1..6: XOR-accumulates into r[i..i+7]; fresh assign to r[i+8]
 *   Row i=7: all positions already initialized — pure XOR-accumulate
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n8(uint64_t * __restrict__ r,
                   const uint64_t * __restrict__ a,
                   const uint64_t * __restrict__ b)
{
    uint64_t lo, hi;

    /* ── Row i=0: assign-on-first-write (initializes r[0..8]) ─────────── */
    clmul64(a[0], b[0], &lo, &hi);
    r[0] = lo;
    r[1] = hi;

    for (size_t j = 1; j < 8; j++) {
        clmul64(a[0], b[j], &lo, &hi);
        r[j]     ^= lo;
        r[j + 1]  = hi;
    }

    /* ── Rows i=1..6: XOR-accumulate with compact inner loop ─────────── */
    for (size_t i = 1; i < 7; i++) {
        for (size_t j = 0; j < 7; j++) {
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
        /* Last column j=7: r[i+8] is a fresh assign */
        clmul64(a[i], b[7], &lo, &hi);
        r[i + 7] ^= lo;
        r[i + 8]  = hi;
    }

    /* ── Row i=7: all positions already initialized ──────────────────── */
    for (size_t j = 0; j < 7; j++) {
        clmul64(a[7], b[j], &lo, &hi);
        r[7 + j]     ^= lo;
        r[7 + j + 1] ^= hi;
    }
    /* Last column j=7 */
    clmul64(a[7], b[7], &lo, &hi);
    r[14] ^= lo;
    r[15]  = hi;
}

/* ── Fixed-size leaf: n=14 (compact loops, assign-on-first-write) ────────
 *
 * Assign-on-first-write discipline:
 *   Row i=0: r[0..14] first-written via compact loop
 *   Row i=1..12: XOR-accumulates into r[i..i+13]; fresh assign to r[i+14]
 *   Row i=13: all positions already initialized — pure XOR-accumulate
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n14(uint64_t * __restrict__ r,
                    const uint64_t * __restrict__ a,
                    const uint64_t * __restrict__ b)
{
    uint64_t lo, hi;

    /* ── Row i=0: assign-on-first-write (initializes r[0..14]) ────────── */
    clmul64(a[0], b[0], &lo, &hi);
    r[0] = lo;
    r[1] = hi;

    for (size_t j = 1; j < 14; j++) {
        clmul64(a[0], b[j], &lo, &hi);
        r[j]     ^= lo;
        r[j + 1]  = hi;
    }

    /* ── Rows i=1..12: XOR-accumulate with compact inner loop ─────────── */
    for (size_t i = 1; i < 13; i++) {
        for (size_t j = 0; j < 13; j++) {
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
        /* Last column j=13: r[i+14] is a fresh assign */
        clmul64(a[i], b[13], &lo, &hi);
        r[i + 13] ^= lo;
        r[i + 14]  = hi;
    }

    /* ── Row i=13: all positions already initialized ─────────────────── */
    for (size_t j = 0; j < 13; j++) {
        clmul64(a[13], b[j], &lo, &hi);
        r[13 + j]     ^= lo;
        r[13 + j + 1] ^= hi;
    }
    /* Last column j=13 */
    clmul64(a[13], b[13], &lo, &hi);
    r[26] ^= lo;
    r[27]  = hi;
}


/* ── Fixed-size leaf: n=15 (compact loops, assign-on-first-write) ────────
 *
 * Assign-on-first-write discipline:
 *   Row i=0: r[0..15] first-written via compact loop
 *   Row i=1..13: XOR-accumulates into r[i..i+14]; fresh assign to r[i+15]
 *   Row i=14: all positions already initialized — pure XOR-accumulate
 */
static __attribute__((always_inline)) inline
void clmul_leaf_n15(uint64_t * __restrict__ r,
                    const uint64_t * __restrict__ a,
                    const uint64_t * __restrict__ b)
{
    uint64_t lo, hi;

    /* ── Row i=0: assign-on-first-write (initializes r[0..15]) ────────── */
    clmul64(a[0], b[0], &lo, &hi);
    r[0] = lo;
    r[1] = hi;

    for (size_t j = 1; j < 15; j++) {
        clmul64(a[0], b[j], &lo, &hi);
        r[j]     ^= lo;
        r[j + 1]  = hi;
    }

    /* ── Rows i=1..13: XOR-accumulate with compact inner loop ─────────── */
    for (size_t i = 1; i < 14; i++) {
        for (size_t j = 0; j < 14; j++) {
            clmul64(a[i], b[j], &lo, &hi);
            r[i + j]     ^= lo;
            r[i + j + 1] ^= hi;
        }
        /* Last column j=14: r[i+15] is a fresh assign */
        clmul64(a[i], b[14], &lo, &hi);
        r[i + 14] ^= lo;
        r[i + 15]  = hi;
    }

    /* ── Row i=14: all positions already initialized ─────────────────── */
    for (size_t j = 0; j < 14; j++) {
        clmul64(a[14], b[j], &lo, &hi);
        r[14 + j]     ^= lo;
        r[14 + j + 1] ^= hi;
    }
    /* Last column j=14 */
    clmul64(a[14], b[14], &lo, &hi);
    r[28] ^= lo;
    r[29]  = hi;
}

#endif /* HQC_CLMUL_LEAF_SPECIALIZED_H */
