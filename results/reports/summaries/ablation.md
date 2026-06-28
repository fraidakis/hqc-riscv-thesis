# Scalar Karatsuba Ablation Study

Generated: 2026-06-28 13:13
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
| `scalar_ref` | (none) | 11,631,706 | ×1.0 | — | 18,258,930 | ×1.0 | — |
| `scalar_zb_A` | A | 361,661 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AC` | A+C | 158,779 | ×73.3 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb` | A+C+B | 130,431 | ×89.2 | ×1.2 | 190,293 | ×96.0 | ×1.3 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,375,745 | ×1.0 | — | 55,517,375 | ×1.0 | — |
| `scalar_zb_A` | A | 1,091,270 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AC` | A+C | 474,430 | ×74.6 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb` | A+C+B | 378,333 | ×93.5 | ×1.3 | 577,452 | ×96.1 | ×1.3 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,378,218 | ×1.0 | — | 137,989,022 | ×1.0 | — |
| `scalar_zb_A` | A | 2,401,836 | ×36.8 | ×36.8 | 3,042,809 | ×45.3 | ×45.3 |
| `scalar_zb_AC` | A+C | 936,047 | ×94.4 | ×2.6 | 1,484,829 | ×92.9 | ×2.0 |
| `scalar_zb` | A+C+B | 796,614 | ×110.9 | ×1.2 | 1,282,136 | ×107.6 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.16 | ×32.42 | ×36.80 |
| A→AC | +specialized recursion tree | ×2.28 | ×2.30 | ×2.57 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.22 | ×1.25 | ×1.18 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.62 | ×38.36 | ×45.35 |
| A→AC | +specialized recursion tree | ×1.99 | ×1.99 | ×2.05 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.25 | ×1.26 | ×1.16 |

