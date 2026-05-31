/**
 * dudect_ttest.h — Streaming Welch's t-test for dudect constant-time verification.
 *
 * Header-only implementation of an online (streaming) Welch's t-test.
 * Maintains running count, sum, and sum-of-squares for each of two input classes,
 * allowing O(1) updates per measurement and O(1) t-statistic computation without
 * storing individual samples.
 *
 * Validates: Requirements 4.1, 4.2, 4.3
 */

#ifndef DUDECT_TTEST_H
#define DUDECT_TTEST_H

#include <stdint.h>
#include <math.h>

/**
 * Online (streaming) statistics accumulator for one input class.
 * Maintains count, sum, and sum-of-squares for O(1) updates.
 */
typedef struct {
    double n;       /* number of measurements */
    double sum;     /* sum of measurements */
    double sum_sq;  /* sum of squares of measurements */
} ttest_acc_t;

/**
 * Paired t-test context: two accumulators (class A, class B).
 */
typedef struct {
    ttest_acc_t class[2];  /* [0] = class A (fixed), [1] = class B (random) */
} ttest_ctx_t;

/**
 * Initialize a t-test context, zeroing both class accumulators.
 */
static inline void ttest_init(ttest_ctx_t *ctx)
{
    ctx->class[0] = (ttest_acc_t){0.0, 0.0, 0.0};
    ctx->class[1] = (ttest_acc_t){0.0, 0.0, 0.0};
}

/**
 * Feed a single timing measurement into the appropriate class accumulator.
 * O(1) time, no memory allocation.
 *
 * @param ctx         The t-test context
 * @param measurement The timing value (e.g., cycle count cast to double)
 * @param class_id    0 for class A (fixed), 1 for class B (random)
 */
static inline void ttest_push(ttest_ctx_t *ctx, double measurement, int class_id)
{
    ttest_acc_t *acc = &ctx->class[class_id];
    acc->n     += 1.0;
    acc->sum   += measurement;
    acc->sum_sq += measurement * measurement;
}

/**
 * Compute Welch's t-statistic from running accumulators.
 * Returns 0.0 if either class has fewer than 2 samples (insufficient data).
 * Guards against near-zero denominator (< 1e-12) to avoid division instability.
 *
 * Formula: t = (mean_A - mean_B) / sqrt(var_A/n_A + var_B/n_B)
 * where var_X = (sum_sq_X - sum_X^2 / n_X) / (n_X - 1)  [sample variance]
 *
 * @param ctx  The t-test context (read-only)
 * @return     Welch's t-statistic, or 0.0 if insufficient data or degenerate case
 */
static inline double ttest_compute(const ttest_ctx_t *ctx)
{
    const ttest_acc_t *a = &ctx->class[0];
    const ttest_acc_t *b = &ctx->class[1];

    /* Require at least 2 samples per class for variance computation */
    if (a->n < 2.0 || b->n < 2.0)
        return 0.0;

    double mean_a = a->sum / a->n;
    double mean_b = b->sum / b->n;
    double var_a  = (a->sum_sq - a->sum * a->sum / a->n) / (a->n - 1.0);
    double var_b  = (b->sum_sq - b->sum * b->sum / b->n) / (b->n - 1.0);

    double denom = sqrt(var_a / a->n + var_b / b->n);

    /* Guard against near-zero denominator */
    if (denom < 1e-12)
        return 0.0;

    return (mean_a - mean_b) / denom;
}

#endif /* DUDECT_TTEST_H */
