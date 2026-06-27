# Scalar Karatsuba Ablation Study

Generated: 2026-06-27 15:50
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_ref` (bit-serial schoolbook, memset, n>>1 split)

Optimizations:
- **A** — Word-level carry-less multiply (Zbc `clmul`)
- **B** — Assign-on-first-write (memset elimination)
- **C** — Compile-time specialized recursion tree
- **D** — Fixed-size fully-unrolled leaf specializations (requires C)

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 11,636,921 | ×1.0 | — | 18,260,929 | ×1.0 | — |
| `scalar_zb_A` | A | 361,181 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AB` | A+B | 340,462 | ×34.2 | ×1.1 | 443,424 | ×41.2 | ×1.1 |
| `scalar_zb_AC` | A+C | 158,442 | ×73.4 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb_ACD` | A+C+D | 147,017 | ×79.2 | ×1.1 | 202,398 | ×90.2 | ×1.2 |
| `scalar_zb` | A+B+C+D | 132,547 | ×87.8 | ×1.1 | 180,508 | ×101.2 | ×1.1 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,383,228 | ×1.0 | — | 55,516,720 | ×1.0 | — |
| `scalar_zb_A` | A | 1,092,487 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AB` | A+B | 1,023,127 | ×34.6 | ×1.1 | 1,360,084 | ×40.8 | ×1.1 |
| `scalar_zb_AC` | A+C | 474,531 | ×74.6 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb_ACD` | A+C+D | 449,319 | ×78.7 | ×1.1 | 628,274 | ×88.4 | ×1.2 |
| `scalar_zb` | A+B+C+D | 401,484 | ×88.1 | ×1.1 | 558,081 | ×99.5 | ×1.1 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,422,662 | ×1.0 | — | 138,003,709 | ×1.0 | — |
| `scalar_zb_A` | A | 2,402,987 | ×36.8 | ×36.8 | 3,043,973 | ×45.3 | ×45.3 |
| `scalar_zb_AB` | A+B | 2,294,006 | ×38.5 | ×1.0 | 2,904,770 | ×47.5 | ×1.0 |
| `scalar_zb_AC` | A+C | 938,174 | ×94.2 | ×2.6 | 1,485,428 | ×92.9 | ×2.0 |
| `scalar_zb_ACD` | A+C+D | 934,904 | ×94.6 | ×1.0 | 1,484,865 | ×92.9 | ×1.0 |
| `scalar_zb` | A+B+C+D | 873,545 | ×101.2 | ×1.1 | 1,397,486 | ×98.8 | ×1.1 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.22 | ×32.39 | ×36.80 |
| A→AB | +assign-on-first-write | ×1.06 | ×1.07 | ×1.05 |
| A→AC | +specialized recursion tree | ×2.28 | ×2.30 | ×2.56 |
| AC→ACD | +leaf unrolling | ×1.08 | ×1.06 | ×1.00 |
| ACD→ABCD | +noinit (on top of A+C+D) | ×1.11 | ×1.12 | ×1.07 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.62 | ×38.36 | ×45.34 |
| A→AB | +assign-on-first-write | ×1.07 | ×1.06 | ×1.05 |
| A→AC | +specialized recursion tree | ×1.99 | ×1.99 | ×2.05 |
| AC→ACD | +leaf unrolling | ×1.18 | ×1.16 | ×1.00 |
| ACD→ABCD | +noinit (on top of A+C+D) | ×1.12 | ×1.13 | ×1.06 |

