# Scalar Karatsuba Ablation Study

Generated: 2026-06-28 22:17
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_ref` (bit-serial schoolbook, memset, n>>1 split)

Optimizations:
- **A** — Word-level carry-less multiply (Zbc `clmul`)
- **C** — Compile-time specialized recursion tree
- **B** — Assign-on-first-write policy (tree recomposition + leaf specializations)

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 11,633,426 | ×1.0 | — | 18,258,045 | ×1.0 | — |
| `scalar_zb_A` | A | 361,196 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AC` | A+C | 158,553 | ×73.4 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb` | A+C+B | 124,149 | ×93.7 | ×1.3 | 187,440 | ×97.4 | ×1.3 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,371,494 | ×1.0 | — | 55,510,145 | ×1.0 | — |
| `scalar_zb_A` | A | 1,092,249 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AC` | A+C | 474,361 | ×74.6 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb` | A+C+B | 366,152 | ×96.6 | ×1.3 | 570,577 | ×97.3 | ×1.3 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,372,259 | ×1.0 | — | 137,984,896 | ×1.0 | — |
| `scalar_zb_A` | A | 2,403,220 | ×36.8 | ×36.8 | 3,043,092 | ×45.3 | ×45.3 |
| `scalar_zb_AC` | A+C | 938,139 | ×94.2 | ×2.6 | 1,484,930 | ×92.9 | ×2.0 |
| `scalar_zb` | A+C+B | 788,663 | ×112.1 | ×1.2 | 1,258,947 | ×109.6 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.21 | ×32.38 | ×36.77 |
| A→AC | +specialized recursion tree | ×2.28 | ×2.30 | ×2.56 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.28 | ×1.30 | ×1.19 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.61 | ×38.35 | ×45.34 |
| A→AC | +specialized recursion tree | ×1.99 | ×1.99 | ×2.05 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.27 | ×1.27 | ×1.18 |

