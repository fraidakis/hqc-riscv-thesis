# Vector RVV Ablation Study (Tier 2)

Generated: 2026-06-28 13:25
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
| `scalar_zb` | (none — Tier 1 champion) | 130,346 | ×1.0 | — | 190,293 | ×1.0 | — |
| `vector_rvv_D` | D | 126,197 | ×1.0 | ×1.0 | 182,379 | ×1.0 | ×1.0 |
| `vector_rvv_V` | ? | 131,231 | ×1.0 | — | 155,322 | ×1.2 | — |
| `vector_rvv_VD` | ? | 121,629 | ×1.1 | — | 155,267 | ×1.2 | — |
| `vector_rvv_DV` | D+V | 122,277 | ×1.1 | ×1.0 | 155,267 | ×1.2 | ×1.2 |
| `vector_rvv` | D+V+R | 120,397 | ×1.1 | ×1.0 | 152,380 | ×1.2 | ×1.0 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 378,783 | ×1.0 | — | 577,452 | ×1.0 | — |
| `vector_rvv_D` | D | 373,781 | ×1.0 | ×1.0 | 554,335 | ×1.0 | ×1.0 |
| `vector_rvv_V` | ? | 375,785 | ×1.0 | — | 457,000 | ×1.3 | — |
| `vector_rvv_VD` | ? | 357,255 | ×1.1 | — | 457,095 | ×1.3 | — |
| `vector_rvv_DV` | D+V | 360,395 | ×1.1 | ×1.0 | 457,095 | ×1.3 | ×1.2 |
| `vector_rvv` | D+V+R | 355,120 | ×1.1 | ×1.0 | 451,246 | ×1.3 | ×1.0 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 797,473 | ×1.0 | — | 1,282,136 | ×1.0 | — |
| `vector_rvv_D` | D | 844,874 | ×0.9 | ×0.9 | 1,345,740 | ×1.0 | ×1.0 |
| `vector_rvv_V` | ? | 839,956 | ×0.9 | — | 1,102,766 | ×1.2 | — |
| `vector_rvv_VD` | ? | 759,640 | ×1.0 | — | 1,008,635 | ×1.3 | — |
| `vector_rvv_DV` | D+V | 760,668 | ×1.0 | ×1.1 | 1,008,635 | ×1.3 | ×1.3 |
| `vector_rvv` | D+V+R | 756,615 | ×1.1 | ×1.0 | 999,244 | ×1.3 | ×1.0 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→D | +direct-write + overlap-save (eliminate copy loops) | ×1.03 | ×1.01 | ×0.94 |
| D→DV | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.03 | ×1.04 | ×1.11 |
| DV→DVR | +RVV-vectorized modular reduction | ×1.02 | ×1.01 | ×1.01 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→D | +direct-write + overlap-save (eliminate copy loops) | ×1.04 | ×1.04 | ×0.95 |
| D→DV | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.17 | ×1.21 | ×1.33 |
| DV→DVR | +RVV-vectorized modular reduction | ×1.02 | ×1.01 | ×1.01 |

## Comparison: Tier 2 champion vs Tier 1 champion

| Level | `scalar_zb` (Tier 1) | `vector_rvv` (Tier 2) | RVV speedup |
|-------|---------------------:|----------------------:|------------:|
| HQC-128 | 184,108 | 146,267 | ×1.26 |
| HQC-192 | 571,251 | 446,658 | ×1.28 |
| HQC-256 | 1,273,112 | 989,721 | ×1.29 |

