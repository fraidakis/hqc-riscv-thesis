# Scalar Karatsuba Ablation Study

Generated: 2026-06-28 12:39
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
| `scalar_ref` | (none) | 11,632,671 | ×1.0 | — | 18,259,056 | ×1.0 | — |
| `scalar_zb_A` | A | 361,381 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AC` | A+C | 157,700 | ×73.8 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb_ACB` | A+C+B | 129,898 | ×89.6 | ×1.2 | 190,293 | ×96.0 | ×1.3 |
| `scalar_zb` | A+C+B | 130,139 | ×89.4 | ×1.2 | 190,293 | ×96.0 | ×1.3 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,374,544 | ×1.0 | — | 55,511,364 | ×1.0 | — |
| `scalar_zb_A` | A | 1,091,707 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AC` | A+C | 474,238 | ×74.6 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb_ACB` | A+C+B | 385,463 | ×91.8 | ×1.2 | 577,452 | ×96.1 | ×1.3 |
| `scalar_zb` | A+C+B | 383,320 | ×92.3 | ×1.2 | 577,452 | ×96.1 | ×1.3 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,387,043 | ×1.0 | — | 137,986,024 | ×1.0 | — |
| `scalar_zb_A` | A | 2,402,167 | ×36.8 | ×36.8 | 3,043,066 | ×45.3 | ×45.3 |
| `scalar_zb_AC` | A+C | 936,209 | ×94.4 | ×2.6 | 1,485,167 | ×92.9 | ×2.0 |
| `scalar_zb_ACB` | A+C+B | 798,261 | ×110.7 | ×1.2 | 1,282,136 | ×107.6 | ×1.2 |
| `scalar_zb` | A+C+B | 795,379 | ×111.1 | ×1.2 | 1,282,136 | ×107.6 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.19 | ×32.40 | ×36.79 |
| A→AC | +specialized recursion tree | ×2.29 | ×2.30 | ×2.57 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.21 | ×1.23 | ×1.17 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.62 | ×38.35 | ×45.34 |
| A→AC | +specialized recursion tree | ×1.99 | ×1.99 | ×2.05 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.25 | ×1.26 | ×1.16 |

