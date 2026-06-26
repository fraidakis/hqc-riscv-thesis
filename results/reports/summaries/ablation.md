# Scalar Karatsuba Ablation Study

Generated: 2026-06-26 14:20
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_ref` (bit-serial schoolbook, memset, n>>1 split)

Optimizations:
- **A** — Word-level carry-less multiply (Zbc `clmul`)
- **B** — Assign-on-first-write (memset elimination)
- **C** — Compile-time specialized recursion tree

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 11,634,945 | ×1.0 | — | 18,258,938 | ×1.0 | — |
| `scalar_zb_A` | A | 361,318 | ×32.2 | ×32.2 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AB` | A+B | 339,660 | ×34.3 | ×1.1 | 443,424 | ×41.2 | ×1.1 |
| `scalar_zb_AC` | A+C | 159,342 | ×73.0 | ×2.3 | 237,931 | ×76.7 | ×2.0 |
| `scalar_zb` | A+B+C | 148,686 | ×78.3 | ×1.1 | 219,621 | ×83.1 | ×1.1 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,383,196 | ×1.0 | — | 55,512,073 | ×1.0 | — |
| `scalar_zb_A` | A | 1,091,659 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AB` | A+B | 1,026,059 | ×34.5 | ×1.1 | 1,360,044 | ×40.8 | ×1.1 |
| `scalar_zb_AC` | A+C | 474,429 | ×74.6 | ×2.3 | 726,069 | ×76.5 | ×2.0 |
| `scalar_zb` | A+B+C | 436,701 | ×81.0 | ×1.1 | 667,579 | ×83.2 | ×1.1 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,404,765 | ×1.0 | — | 137,989,589 | ×1.0 | — |
| `scalar_zb_A` | A | 2,402,095 | ×36.8 | ×36.8 | 3,042,942 | ×45.3 | ×45.3 |
| `scalar_zb_AB` | A+B | 2,287,774 | ×38.6 | ×1.0 | 2,903,514 | ×47.5 | ×1.0 |
| `scalar_zb_AC` | A+C | 936,051 | ×94.4 | ×2.6 | 1,485,797 | ×92.9 | ×2.0 |
| `scalar_zb` | A+B+C | 874,300 | ×101.1 | ×1.1 | 1,397,486 | ×98.7 | ×1.1 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 Δ | HQC-192 Δ | HQC-256 Δ |
|------------|--------------|----------:|----------:|----------:|
| ref→A | +word-level clmul | -96.9% | -96.9% | -97.3% |
| A→AB | +assign-on-first-write | -6.0% | -6.0% | -4.8% |
| A→AC | +specialized recursion tree | -55.9% | -56.5% | -61.0% |
| AC→ABC | +noinit (on top of A+C) | -6.7% | -8.0% | -6.6% |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 Δ | HQC-192 Δ | HQC-256 Δ |
|------------|--------------|----------:|----------:|----------:|
| ref→A | +word-level clmul | -97.4% | -97.4% | -97.8% |
| A→AB | +assign-on-first-write | -6.2% | -6.0% | -4.6% |
| A→AC | +specialized recursion tree | -49.7% | -49.8% | -51.2% |
| AC→ABC | +noinit (on top of A+C) | -7.7% | -8.1% | -5.9% |

