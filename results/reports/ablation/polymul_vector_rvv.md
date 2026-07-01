# Vector RVV Ablation Study (Tier 2)

Generated: 2026-06-30 22:45
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `scalar_zb` (Tier 1 champion — includes direct-write discipline + overlap-save)

Optimizations (layered incrementally on top of scalar_zb):
- **V** — RVV-vectorized recomposition (inline LMUL-tuned XOR/copy at upper tree levels)

## HQC-128

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 122,503 | ×1.0 | — | 187,440 | ×1.0 | — |
| `vector_rvv_V` | V | 120,960 | ×1.0 | ×1.0 | 157,287 | ×1.2 | ×1.2 |

## HQC-192

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 367,480 | ×1.0 | — | 570,577 | ×1.0 | — |
| `vector_rvv_V` | V | 353,892 | ×1.0 | ×1.0 | 463,739 | ×1.2 | ×1.2 |

## HQC-256

| Variant | Optimizations | median cycles | vs ref | vs prev | instret | vs ref | vs prev |
|---------|---------------|-------------:|-------:|--------:|--------:|-------:|--------:|
| `scalar_zb` | (none — Tier 1 champion) | 790,047 | ×1.0 | — | 1,258,947 | ×1.0 | — |
| `vector_rvv_V` | V | 747,626 | ×1.1 | ×1.1 | 1,022,645 | ×1.2 | ×1.2 |

## Incremental Optimization Contribution (cycles)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→V | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.01 | ×1.04 | ×1.06 |

## Incremental Optimization Contribution (instret)

| Transition | What changes | HQC-128 | HQC-192 | HQC-256 |
|------------|--------------|--------:|--------:|--------:|
| ref→V | +RVV-vectorized recomposition (inline LMUL-tuned) | ×1.19 | ×1.23 | ×1.23 |

## Comparison: Tier 2 champion vs Tier 1 champion

| Level | `scalar_zb` (Tier 1) | `vector_rvv_V` (Tier 2) | RVV speedup |
|-------|---------------------:|----------------------:|------------:|
| HQC-128 | 182,726 | 148,479 | ×1.23 |
| HQC-192 | 570,293 | 449,808 | ×1.27 |
| HQC-256 | 1,262,888 | 1,043,726 | ×1.21 |

