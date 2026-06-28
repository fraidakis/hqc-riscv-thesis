# Working Set Analysis: L1 Cache Constraints

**Target:** SpacemiT X60 (BPI-F3), 64 KB L1 data cache  
**Date:** 2025  
**Requirements:** 9.3, 9.4

## Summary

The simultaneously-active working set for `vect_mul` fits within the 64 KB L1 data
cache for all three HQC security levels (128, 192, 256). No restructuring is needed.

## Memory Layout

With the RVV direct-write discipline, `vect_mul` allocates two stack buffers:

```c
uint64_t unreduced[2 * VEC_N_SIZE_64];   // output of Karatsuba tree
uint64_t tmp_buf[4 * VEC_N_SIZE_64];     // shared scratch across all recursion levels
```

The input arrays `a` and `b` (each `VEC_N_SIZE_64` words) are passed by pointer and
are read-only.

## Recursion Structure

The Karatsuba tree executes **sequentially** at each level:
1. `kmul_n0(r, a, b, child_buf)` — computes z0 directly into `r[0..2*n0-1]`
2. `kmul_n1(r+2*n0, a+n0, b+n0, child_buf)` — computes z2 directly into `r[2*n0..]`
3. ta/tb XOR preparation — reads from `a`, `b`; writes to `tmp[zmid+2*n0..]`
4. `kmul_n0(zmid, ta, tb, child_buf)` — computes zmid into `tmp[0..2*n0-1]`
5. XOR accumulation — reads `r`, `zmid`, `ta`(saved); writes `r`

Since child calls are sequential (not concurrent), `child_buf` is reused across all
three child calls at each level. The simultaneously-active data at any point is
determined by which phase is executing.

## Per-Level tmp Layout

At each recursion level with operand size `n` (where `n0 = ceil(n/2)`, `n1 = floor(n/2)`):

```
tmp: [zmid: 2*n0 words][ta: n0 words][tb: n0 words][child_buf: ...]
```

Per-level cost: `2*n0 + n0 + n0 = 4*n0 ≈ 2*n + 2` words.

## Simultaneously-Active Working Set Analysis

The critical question is: what data must be in cache *at the same time*?

### Phase Analysis (per recursion level, operand size n)

**Phase 1-2: Child calls (z0, z2)**
- Active: input slice of `a` (n0 words), input slice of `b` (n0 words), output region of `r` (2*n0 words)
- The child call recurses deeper, but at the leaf level the active set is just the leaf inputs/outputs
- Working set: dominated by the deepest active frame

**Phase 3: ta/tb preparation**
- Active: `a[0..n0-1]`, `a[n0..n-1]`, `b[0..n0-1]`, `b[n0..n-1]` (read), `ta`, `tb` (write)
- Size: 4*n0 (read) + 2*n0 (write) = 6*n0 words × 8 bytes

**Phase 4: zmid child call**
- Active: `ta` (n0 words), `tb` (n0 words), `zmid` (2*n0 words), child recursion
- Size: 4*n0 words × 8 bytes (plus deeper recursion)

**Phase 5: XOR accumulation (the peak)**
- Active: `r[0..2*n-1]` (read-modify-write), `zmid[0..2*n0-1]` (read), `ta[0..n0-1]` (saved overlap, read)
- Size: 2*n + 2*n0 + n0 = 2*n + 3*n0 ≈ 3.5*n words × 8 bytes
- This is the **peak working set** at any single level

### Key Insight: Sequential Execution Limits Simultaneity

At any given moment during execution, only ONE recursion level is actively processing
its XOR accumulation phase. Deeper levels have already completed and their tmp regions
are no longer needed. The parent level's XOR accumulation hasn't started yet.

The simultaneously-active data during the XOR accumulation at level L is:
- The current level's `r` region: `2*n_L` words
- The current level's `zmid`: `2*n0_L` words  
- The current level's saved `ta`: `n0_L` words
- Input arrays `a`, `b` (portions being read for parent's future phases): **not active**

## HQC-128 (VEC_N_SIZE_64 = 277, Threshold T = 16)

### Recursion Tree
```
Level 0: kmul_277  (n=277, n0=139, n1=138)
Level 1: kmul_139  (n=139, n0=70,  n1=69)
Level 2: kmul_70   (n=70,  n0=35,  n1=35)
Level 3: kmul_35   (n=35,  n0=18,  n1=17)
Level 4: kmul_18   (n=18,  n0=9,   n1=9)  → leaf calls clmul_leaf(9)
```

### Top-Level Working Set (vect_mul frame)

| Buffer | Words | Bytes |
|--------|-------|-------|
| `a` (input, read-only) | 277 | 2,216 |
| `b` (input, read-only) | 277 | 2,216 |
| `unreduced` (output) | 554 | 4,432 |
| `tmp_buf` (shared scratch) | 1,108 | 8,864 |
| **Total allocated** | **2,216** | **17,728** |

### Peak Simultaneously-Active Set (XOR accumulation at Level 0)

During `kmul_277`'s XOR accumulation phase:

| Data Region | Words | Bytes | Access Pattern |
|-------------|-------|-------|----------------|
| `r[0..553]` (unreduced) | 554 | 4,432 | Read-modify-write |
| `zmid[0..277]` in tmp | 278 | 2,224 | Read |
| `ta[0..138]` saved overlap | 139 | 1,112 | Read |
| **Simultaneously active** | **971** | **7,768** |

Note: `a` and `b` are NOT accessed during XOR accumulation — they were only needed
for ta/tb preparation (Phase 3) which has already completed.

### Peak at Deeper Levels

| Level | n | Peak Active (bytes) | Cumulative with parent r |
|-------|---|--------------------|-----------------------|
| 0 | 277 | 7,768 | 7,768 |
| 1 | 139 | 3,920 | — (within parent's r) |
| 2 | 70 | 1,960 | — |
| 3 | 35 | 1,008 | — |
| 4 | 18 | 504 | — |

The deeper levels operate on subsets of the same `r` buffer (direct-write discipline),
so their working set is a subset of the top level's `r` region — no additional cache
pressure.

### Worst-Case Total (all data that might be touched)

Even counting ALL allocated memory (not simultaneously active):
- Total: 17,728 bytes = **17.3 KB** ≪ 64 KB ✓

## HQC-192 (VEC_N_SIZE_64 = 561, Threshold T = 16)

### Recursion Tree
```
Level 0: kmul_561  (n=561, n0=281, n1=280)
Level 1: kmul_281  (n=281, n0=141, n1=140)
Level 2: kmul_141  (n=141, n0=71,  n1=70)
Level 3: kmul_71   (n=71,  n0=36,  n1=35)
Level 4: kmul_36   (n=36,  n0=18,  n1=18)  → leaf calls
```

### Top-Level Working Set

| Buffer | Words | Bytes |
|--------|-------|-------|
| `a` (input, read-only) | 561 | 4,488 |
| `b` (input, read-only) | 561 | 4,488 |
| `unreduced` (output) | 1,122 | 8,976 |
| `tmp_buf` (shared scratch) | 2,244 | 17,952 |
| **Total allocated** | **4,488** | **35,904** |

### Peak Simultaneously-Active Set (XOR accumulation at Level 0)

| Data Region | Words | Bytes | Access Pattern |
|-------------|-------|-------|----------------|
| `r[0..1121]` (unreduced) | 1,122 | 8,976 | Read-modify-write |
| `zmid[0..561]` in tmp | 562 | 4,496 | Read |
| `ta[0..280]` saved overlap | 281 | 2,248 | Read |
| **Simultaneously active** | **1,965** | **15,720** |

### Worst-Case Total

Total allocated: 35,904 bytes = **35.1 KB** ≪ 64 KB ✓

## HQC-256 (VEC_N_SIZE_64 = 901, Threshold T = 16)

### Recursion Tree
```
Level 0: kmul_901  (n=901, n0=451, n1=450)
Level 1: kmul_451  (n=451, n0=226, n1=225)
Level 2: kmul_226  (n=226, n0=113, n1=113)
Level 3: kmul_113  (n=113, n0=57,  n1=56)
Level 4: kmul_57   (n=57,  n0=29,  n1=28)
Level 5: kmul_29   (n=29,  n0=15,  n1=14) → leaf calls
```

### Top-Level Working Set

| Buffer | Words | Bytes |
|--------|-------|-------|
| `a` (input, read-only) | 901 | 7,208 |
| `b` (input, read-only) | 901 | 7,208 |
| `unreduced` (output) | 1,802 | 14,416 |
| `tmp_buf` (shared scratch) | 3,604 | 28,832 |
| **Total allocated** | **7,208** | **57,664** |

### Peak Simultaneously-Active Set (XOR accumulation at Level 0)

| Data Region | Words | Bytes | Access Pattern |
|-------------|-------|-------|----------------|
| `r[0..1801]` (unreduced) | 1,802 | 14,416 | Read-modify-write |
| `zmid[0..901]` in tmp | 902 | 7,216 | Read |
| `ta[0..450]` saved overlap | 451 | 3,608 | Read |
| **Simultaneously active** | **3,155** | **25,240** |

### Worst-Case Total (conservative upper bound)

If we assume ALL allocated data might be in cache simultaneously (unrealistic but
provides an upper bound):

Total allocated: 57,664 bytes = **56.3 KB** < 64 KB ✓

Even this pessimistic bound fits within L1. In practice, the simultaneously-active
set during any single phase is much smaller (25.2 KB peak).

## Reduction Phase Working Set

After the Karatsuba tree completes, `reduce` processes:
- Input: `unreduced[0..2*VEC_N_SIZE_64-1]` (read)
- Output: `o[0..VEC_N_SIZE_64-1]` (write)

| Level | Input (bytes) | Output (bytes) | Total |
|-------|--------------|----------------|-------|
| HQC-128 | 4,432 | 2,216 | 6,648 |
| HQC-192 | 8,976 | 4,488 | 13,464 |
| HQC-256 | 14,416 | 7,208 | 21,624 |

All well within 64 KB.

## Conclusion

| Security Level | Total Allocated | Peak Simultaneous | Fits L1 (64 KB)? |
|---------------|----------------|-------------------|-------------------|
| HQC-128 | 17.3 KB | 7.6 KB | ✓ Yes |
| HQC-192 | 35.1 KB | 15.3 KB | ✓ Yes |
| HQC-256 | 56.3 KB | 24.6 KB | ✓ Yes |

**No restructuring is needed.** The working set fits within the 64 KB L1 data cache
for all security levels, even under the most conservative (total allocation) analysis.

The key factors enabling this:
1. **Direct-write discipline** eliminates z0/z2 from tmp (saves ~2×n words per level)
2. **Sequential child execution** means only one recursion path is active at a time
3. **Shared child_buf** reuse across the 3 sequential child calls at each level
4. **`TMP_BUFFER_WORDS = 4 * VEC_N_SIZE_64`** is the tight multiplier (empirically verified)

### Potential Concern: HQC-256 Total Allocation

HQC-256's total allocation (56.3 KB) is close to the 64 KB limit. However:
- The simultaneously-active set is only 24.6 KB (38% of L1)
- The sequential execution model means earlier phases' data naturally evicts before later phases need cache space
- The streaming access pattern of the XOR accumulation (linear scan) is cache-friendly
- No capacity misses are expected under normal operation

If future optimizations increase buffer sizes, HQC-256 should be re-evaluated.
