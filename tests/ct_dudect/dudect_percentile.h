#ifndef DUDECT_PERCENTILE_H
#define DUDECT_PERCENTILE_H

#include <stdint.h>
#include <string.h>

/**
 * Partition helper for quickselect: Lomuto partition scheme.
 * Returns the final position of the pivot.
 */
static inline int _percentile_partition(uint64_t *arr, int lo, int hi)
{
    uint64_t pivot = arr[hi];
    int i = lo;
    for (int j = lo; j < hi; j++) {
        if (arr[j] <= pivot) {
            uint64_t tmp = arr[i];
            arr[i] = arr[j];
            arr[j] = tmp;
            i++;
        }
    }
    uint64_t tmp = arr[i];
    arr[i] = arr[hi];
    arr[hi] = tmp;
    return i;
}

/**
 * Quickselect: find the k-th smallest element in O(n) average time.
 * Modifies the array in-place (partial reordering).
 * Returns the value at position k after partitioning.
 */
static inline uint64_t _percentile_quickselect(uint64_t *arr, int n, int k)
{
    int lo = 0, hi = n - 1;
    while (lo < hi) {
        int pivot_idx = _percentile_partition(arr, lo, hi);
        if (pivot_idx == k)
            return arr[k];
        else if (pivot_idx < k)
            lo = pivot_idx + 1;
        else
            hi = pivot_idx - 1;
    }
    return arr[lo];
}

/**
 * Compute the percentile threshold from a batch of measurements.
 * Uses quickselect (O(n) average) instead of full sort.
 *
 * @param times     Array of timing measurements (partially reordered in place)
 * @param n         Number of measurements
 * @param percentile Percentile to compute (e.g., 95 for top-5% crop)
 * @return          The threshold value at the given percentile
 */
static inline uint64_t percentile_threshold(uint64_t *times, int n, int percentile)
{
    int idx = (int)((double)n * (double)percentile / 100.0);
    if (idx >= n)
        idx = n - 1;

    return _percentile_quickselect(times, n, idx);
}

#endif /* DUDECT_PERCENTILE_H */
