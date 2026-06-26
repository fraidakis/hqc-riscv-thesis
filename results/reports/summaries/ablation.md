# Scalar Karatsuba Ablation Study

Generated: 2026-06-26 15:40
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
| `scalar_ref` | (none) | 11,635,512 | ×1.0 | — | 18,258,834 | ×1.0 | — |
| `scalar_zb_A` | A | 360,659 | ×32.3 | ×32.3 | 472,825 | ×38.6 | ×38.6 |
| `scalar_zb_AB` | A+B | 338,939 | ×34.3 | ×1.1 | 443,424 | ×41.2 | ×1.1 |
| `scalar_zb_AC` | A+C | 147,703 | ×78.8 | ×2.4 | 202,398 | ×90.2 | ×2.3 |
| `scalar_zb` | A+B+C | 132,563 | ×87.8 | ×1.1 | 180,508 | ×101.2 | ×1.1 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 35,383,632 | ×1.0 | — | 55,511,319 | ×1.0 | — |
| `scalar_zb_A` | A | 1,091,491 | ×32.4 | ×32.4 | 1,447,414 | ×38.4 | ×38.4 |
| `scalar_zb_AB` | A+B | 1,026,416 | ×34.5 | ×1.1 | 1,359,807 | ×40.8 | ×1.1 |
| `scalar_zb_AC` | A+C | 447,974 | ×79.0 | ×2.4 | 628,274 | ×88.4 | ×2.3 |
| `scalar_zb` | A+B+C | 397,860 | ×88.9 | ×1.1 | 556,657 | ×99.7 | ×1.1 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_ref` | (none) | 88,401,278 | ×1.0 | — | 137,988,794 | ×1.0 | — |
| `scalar_zb_A` | A | 2,402,469 | ×36.8 | ×36.8 | 3,043,225 | ×45.3 | ×45.3 |
| `scalar_zb_AB` | A+B | 2,290,570 | ×38.6 | ×1.0 | 2,904,530 | ×47.5 | ×1.0 |
| `scalar_zb_AC` | A+C | 951,366 | ×92.9 | ×2.5 | 1,481,973 | ×93.1 | ×2.1 |
| `scalar_zb` | A+B+C | 872,819 | ×101.3 | ×1.1 | 1,397,486 | ×98.7 | ×1.1 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×32.26 | ×32.42 | ×36.79 |
| A→AB | +assign-on-first-write | ×1.06 | ×1.06 | ×1.05 |
| A→AC | +specialized recursion tree + leaf unrolling | ×2.44 | ×2.44 | ×2.53 |
| AC→ABC | +noinit (on top of A+C) | ×1.11 | ×1.13 | ×1.09 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→A | +word-level clmul | ×38.62 | ×38.35 | ×45.34 |
| A→AB | +assign-on-first-write | ×1.07 | ×1.06 | ×1.05 |
| A→AC | +specialized recursion tree + leaf unrolling | ×2.34 | ×2.30 | ×2.05 |
| AC→ABC | +noinit (on top of A+C) | ×1.12 | ×1.13 | ×1.06 |

