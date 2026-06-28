# Vector RVV Ablation Study (Tier 2)

Generated: 2026-06-28 15:46
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_zb` (Tier 1 champion — includes direct-write discipline + overlap-save)

Optimizations (layered incrementally on top of scalar_zb):
- **V** — RVV-vectorized recomposition (inline LMUL-tuned XOR/copy at upper tree levels)

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 124,657 | ×1.0 | — | 188,494 | ×1.0 | — |
| `vector_rvv_V` | V | 122,298 | ×1.0 | ×1.0 | 155,267 | ×1.2 | ×1.2 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 368,634 | ×1.0 | — | 572,432 | ×1.0 | — |
| `vector_rvv_V` | V | 357,665 | ×1.0 | ×1.0 | 457,095 | ×1.3 | ×1.3 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 786,402 | ×1.0 | — | 1,250,925 | ×1.0 | — |
| `vector_rvv_V` | V | 757,818 | ×1.0 | ×1.0 | 1,008,635 | ×1.2 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→V | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.02 | ×1.03 | ×1.04 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→V | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.21 | ×1.25 | ×1.24 |

## Comparison: Tier 2 champion vs Tier 1 champion

| Level | `scalar_zb` (Tier 1) | `vector_rvv_V` (Tier 2) | RVV speedup |
|-------|---------------------:|----------------------:|------------:|
| HQC-128 | 183,738 | 155,267 | ×1.18 |
| HQC-192 | 573,781 | 457,095 | ×1.26 |
| HQC-256 | 1,256,916 | 1,008,635 | ×1.25 |
