# Scalar Karatsuba Ablation Study

Generated: 2026-06-28 16:20
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
| `scalar_ref` | (none) | 11,633,966 | ×1.0 | — | 18,258,617 | ×1.0 | — |
| `scalar_zb_A` | A | 361,826 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AC` | A+C | 158,425 | ×73.4 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb` | A+C+B | 125,027 | ×93.1 | ×1.3 | 188,494 | ×96.9 | ×1.3 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,380,227 | ×1.0 | — | 55,512,075 | ×1.0 | — |
| `scalar_zb_A` | A | 1,092,585 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AC` | A+C | 474,001 | ×74.6 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb` | A+C+B | 367,356 | ×96.3 | ×1.3 | 572,432 | ×97.0 | ×1.3 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,397,726 | ×1.0 | — | 137,999,591 | ×1.0 | — |
| `scalar_zb_A` | A | 2,401,092 | ×36.8 | ×36.8 | 3,043,002 | ×45.3 | ×45.3 |
| `scalar_zb_AC` | A+C | 937,676 | ×94.3 | ×2.6 | 1,484,317 | ×93.0 | ×2.1 |
| `scalar_zb` | A+C+B | 786,317 | ×112.4 | ×1.2 | 1,250,925 | ×110.3 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.15 | ×32.38 | ×36.82 |
| A→AC | +specialized recursion tree | ×2.28 | ×2.31 | ×2.56 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.27 | ×1.29 | ×1.19 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.62 | ×38.35 | ×45.35 |
| A→AC | +specialized recursion tree | ×1.99 | ×1.99 | ×2.05 |
| AC→ACB | +assign-on-first-write (tree + leaf) | ×1.26 | ×1.27 | ×1.19 |

