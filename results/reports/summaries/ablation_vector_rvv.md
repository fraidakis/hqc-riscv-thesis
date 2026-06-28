# Vector RVV Ablation Study (Tier 2)

Generated: 2026-06-28 22:08
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_zb` (Tier 1 champion — includes direct-write discipline + overlap-save)

Optimizations (layered incrementally on top of scalar_zb):
- **V** — RVV-vectorized recomposition (inline LMUL-tuned XOR/copy at upper tree levels)

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 123,341 | ×1.0 | — | 188,494 | ×1.0 | — |
| `vector_rvv_V` | V | 120,960 | ×1.0 | ×1.0 | 157,287 | ×1.2 | ×1.2 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 368,640 | ×1.0 | — | 572,432 | ×1.0 | — |
| `vector_rvv_V` | V | 353,892 | ×1.0 | ×1.0 | 463,739 | ×1.2 | ×1.2 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 786,099 | ×1.0 | — | 1,250,925 | ×1.0 | — |
| `vector_rvv_V` | V | 747,626 | ×1.1 | ×1.1 | 1,022,645 | ×1.2 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→V | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.02 | ×1.04 | ×1.05 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→V | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.20 | ×1.23 | ×1.22 |

## Comparison: Tier 2 champion vs Tier 1 champion

| Level | `scalar_zb` (Tier 1) | `vector_rvv_V` (Tier 2) | RVV speedup |
|-------|---------------------:|----------------------:|------------:|
| HQC-128 | 183,738 | 150,855 | ×1.22 |
| HQC-192 | 573,781 | 457,566 | ×1.25 |
| HQC-256 | 1,256,916 | 1,011,442 | ×1.24 |

