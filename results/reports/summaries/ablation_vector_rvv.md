# Vector RVV Ablation Study (Tier 2)

Generated: 2026-06-26 21:10
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_zb` (Tier 1 champion — copy-loop recomposition, scalar reduce)

Optimizations (layered incrementally on top of scalar_zb):
- **D** — Direct-write discipline + overlap-save (children write directly to output, eliminate copy loops 1+2)
- **V** — RVV-vectorized recomposition (inline LMUL-tuned XOR/copy at upper tree levels)
- **R** — RVV-vectorized modular reduction (`reduce_rvv`)

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 132,544 | ×1.0 | — | 180,508 | ×1.0 | — |
| `vector_rvv_D` | D | 126,418 | ×1.0 | ×1.0 | 174,377 | ×1.0 | ×1.0 |
| `vector_rvv_V` | ? | 132,131 | ×1.0 | — | 147,550 | ×1.2 | — |
| `vector_rvv_VD` | ? | 123,015 | ×1.1 | — | 147,265 | ×1.2 | — |
| `vector_rvv_DV` | D+V | 122,629 | ×1.1 | ×1.0 | 147,265 | ×1.2 | ×1.2 |
| `vector_rvv` | D+V+R | 121,495 | ×1.1 | ×1.0 | 144,378 | ×1.3 | ×1.0 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 402,034 | ×1.0 | — | 557,215 | ×1.0 | — |
| `vector_rvv_D` | D | 383,314 | ×1.0 | ×1.0 | 540,132 | ×1.0 | ×1.0 |
| `vector_rvv_V` | ? | 395,456 | ×1.0 | — | 444,577 | ×1.3 | — |
| `vector_rvv_VD` | ? | 360,472 | ×1.1 | — | 437,042 | ×1.3 | — |
| `vector_rvv_DV` | D+V | 360,494 | ×1.1 | ×1.1 | 437,042 | ×1.3 | ×1.2 |
| `vector_rvv` | D+V+R | 358,802 | ×1.1 | ×1.0 | 431,193 | ×1.3 | ×1.0 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 875,284 | ×1.0 | — | 1,397,486 | ×1.0 | — |
| `vector_rvv_D` | D | 897,532 | ×1.0 | ×1.0 | 1,258,348 | ×1.1 | ×1.1 |
| `vector_rvv_V` | ? | 894,510 | ×1.0 | — | 1,015,462 | ×1.4 | — |
| `vector_rvv_VD` | ? | 848,113 | ×1.0 | — | 1,030,329 | ×1.4 | — |
| `vector_rvv_DV` | D+V | 847,837 | ×1.0 | ×1.1 | 1,030,329 | ×1.4 | ×1.2 |
| `vector_rvv` | D+V+R | 845,108 | ×1.0 | ×1.0 | 1,020,938 | ×1.4 | ×1.0 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→D | +direct-write + overlap-save (eliminate copy loops) | ×1.05 | ×1.05 | ×0.98 |
| D→DV | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.03 | ×1.06 | ×1.06 |
| DV→DVR | +RVV-vectorized modular reduction | ×1.01 | ×1.00 | ×1.00 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→D | +direct-write + overlap-save (eliminate copy loops) | ×1.04 | ×1.03 | ×1.11 |
| D→DV | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.18 | ×1.24 | ×1.22 |
| DV→DVR | +RVV-vectorized modular reduction | ×1.02 | ×1.01 | ×1.01 |

## Comparison: Tier 2 champion vs Tier 1 champion

| Level | `scalar_zb` (Tier 1) | `vector_rvv` (Tier 2) | RVV speedup |
|-------|---------------------:|----------------------:|------------:|
| HQC-128 | 167,852 | 140,487 | ×1.19 |
| HQC-192 | 530,977 | 439,516 | ×1.21 |
| HQC-256 | 1,824,506 | 1,014,499 | ×1.80 |

