# Scalar Karatsuba Ablation Study

Generated: 2026-06-27 18:46
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
| `scalar_ref` | (none) | 11,635,902 | ×1.0 | — | 18,259,984 | ×1.0 | — |
| `scalar_zb_A` | A | 361,848 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AB` | A+B | 341,053 | ×34.1 | ×1.1 | 443,424 | ×41.2 | ×1.1 |
| `scalar_zb_AC` | A+C | 158,085 | ×73.6 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb_ACB` | A+C+B | 146,447 | ×79.5 | ×1.1 | 219,621 | ×83.1 | ×1.1 |
| `scalar_zb_ACD` | A+C+D | 147,044 | ×79.1 | ×1.1 | 204,546 | ×89.3 | ×1.2 |
| `scalar_zb` | A+B+C+D | 131,893 | ×88.2 | ×1.1 | 182,262 | ×100.2 | ×1.1 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,387,207 | ×1.0 | — | 55,517,389 | ×1.0 | — |
| `scalar_zb_A` | A | 1,091,953 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AB` | A+B | 1,025,702 | ×34.5 | ×1.1 | 1,360,085 | ×40.8 | ×1.1 |
| `scalar_zb_AC` | A+C | 473,583 | ×74.7 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb_ACB` | A+C+B | 435,519 | ×81.3 | ×1.1 | 667,579 | ×83.2 | ×1.1 |
| `scalar_zb_ACD` | A+C+D | 447,536 | ×79.1 | ×1.1 | 633,060 | ×87.7 | ×1.1 |
| `scalar_zb` | A+B+C+D | 392,564 | ×90.1 | ×1.1 | 563,598 | ×98.5 | ×1.1 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,408,599 | ×1.0 | — | 137,999,441 | ×1.0 | — |
| `scalar_zb_A` | A | 2,402,361 | ×36.8 | ×36.8 | 3,043,121 | ×45.3 | ×45.3 |
| `scalar_zb_AB` | A+B | 2,289,919 | ×38.6 | ×1.0 | 2,904,676 | ×47.5 | ×1.0 |
| `scalar_zb_AC` | A+C | 935,268 | ×94.5 | ×2.6 | 1,485,662 | ×92.9 | ×2.0 |
| `scalar_zb_ACB` | A+C+B | 874,217 | ×101.1 | ×1.1 | 1,397,486 | ×98.7 | ×1.1 |
| `scalar_zb_ACD` | A+C+D | 939,613 | ×94.1 | ×1.0 | 1,485,031 | ×92.9 | ×1.0 |
| `scalar_zb` | A+B+C+D | 872,731 | ×101.3 | ×1.0 | 1,397,486 | ×98.7 | ×1.0 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.16 | ×32.41 | ×36.80 |
| A→AB | +assign-on-first-write (no tree) | ×1.06 | ×1.06 | ×1.05 |
| A→AC | +specialized recursion tree | ×2.29 | ×2.31 | ×2.57 |
| AC→ACB | +noinit recomposition | ×1.08 | ×1.09 | ×1.07 |
| AC→ACD | +leaf unrolling | ×1.08 | ×1.06 | ×1.00 |
| ACB→ABCD | +leaf unrolling (on top of A+C+B) | ×1.11 | ×1.11 | ×1.00 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.62 | ×38.36 | ×45.35 |
| A→AB | +assign-on-first-write (no tree) | ×1.07 | ×1.06 | ×1.05 |
| A→AC | +specialized recursion tree | ×1.99 | ×1.99 | ×2.05 |
| AC→ACB | +noinit recomposition | ×1.08 | ×1.09 | ×1.06 |
| AC→ACD | +leaf unrolling | ×1.16 | ×1.15 | ×1.00 |
| ACD→ABCD | +noinit (on top of A+C+D) | ×1.12 | ×1.12 | ×1.06 |

