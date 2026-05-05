#ifndef FAFFT_V4_H
#define FAFFT_V4_H

#include <stdint.h>

/*
 * Phase 1b v4 — FAFFT-oriented alternatives.
 *
 * Task 1: Truncated FAFFT (known incorrect for HQC-128 due to 512-word limit).
 * Task 4: RVV vectorization of monomial-basis Gao-Mateer style butterflies.
 */

void vect_mul_v4(uint64_t *o, const uint64_t *a, const uint64_t *b);

/* Task 1: truncated half-domain FAFFT (kept for scoreboard reproducibility). */
void vect_mul_v4_task1(uint64_t *o, const uint64_t *a, const uint64_t *b);

/* Task 2/3 deprecated in this revision (kept as symbols for build compatibility). */
void vect_mul_v4_task2(uint64_t *o, const uint64_t *a, const uint64_t *b);
void vect_mul_v4_task3(uint64_t *o, const uint64_t *a, const uint64_t *b);

/* Task 4: true RVV monomial-basis FAFFT butterfly path. */
void vect_mul_v4_task4_fafft(uint64_t *o, const uint64_t *a, const uint64_t *b);

#endif
