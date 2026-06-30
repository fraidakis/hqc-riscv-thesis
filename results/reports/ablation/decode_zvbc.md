# Zvbc Decode Ablation Study (Tier 3)

Generated: 2026-06-30 19:37
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: dev

Baseline: `rvv` (Tier 2 champion — RVV RM decode + RVV RS syndromes, scalar BM/Forney/FFT)

Optimizations (layered incrementally on top of Tier 2):
- **B+D** — Zvbc-vectorized Berlekamp-Massey (`compute_elp_zvbc`: vclmul.vv SAXPY + vlse16 neg-stride dot-product) + Forney error-values (`compute_error_values_zvbc`: vclmul.vv/vx inner-k batch)
- **C** — Zvbc-vectorized iterative FFT root-finding (`fft_iterative`: recursive→BFS transform, beta_scale + xor_broadcast + gammas_sums_combine helpers)

Note: All steps inherit the Tier 2 RVV RM decode (expand_and_sum + hadamard + find_peaks)
and RVV syndrome computation (Stage A). The Barrett single-pass reduction is embedded
in the Zvbc pipeline — its contribution is included in both B+D and C measurements.

## HQC-128

| Variant | Optimizations | median cycles | vs base | vs prev | instret | vs base | vs prev |
|---------|---------------|-------------:|--------:|--------:|--------:|--------:|--------:|
| `rvv` | (none — Tier 2 RVV baseline) | 205,072 | ×1.00 | — | 134,737 | ×1.00 | — |
| `zvbc_BD` | B+D | — | — | — | 95,543 | ×1.41 | ×1.41 |
| `zvbc` | B+D+C (champion) | — | — | — | 73,769 | ×1.83 | ×1.30 |

## HQC-192

| Variant | Optimizations | median cycles | vs base | vs prev | instret | vs base | vs prev |
|---------|---------------|-------------:|--------:|--------:|--------:|--------:|--------:|
| `rvv` | (none — Tier 2 RVV baseline) | 279,624 | ×1.00 | — | 168,915 | ×1.00 | — |
| `zvbc_BD` | B+D | — | — | — | 124,964 | ×1.35 | ×1.35 |
| `zvbc` | B+D+C (champion) | — | — | — | 89,688 | ×1.88 | ×1.39 |

## HQC-256

| Variant | Optimizations | median cycles | vs base | vs prev | instret | vs base | vs prev |
|---------|---------------|-------------:|--------:|--------:|--------:|--------:|--------:|
| `rvv` | (none — Tier 2 RVV baseline) | 546,616 | ×1.00 | — | 429,339 | ×1.00 | — |
| `zvbc_BD` | B+D | — | — | — | 268,646 | ×1.60 | ×1.60 |
| `zvbc` | B+D+C (champion) | — | — | — | 231,570 | ×1.85 | ×1.16 |

## Incremental Optimization Contribution

| Transition | What changes | Vectorized sub-function |
|------------|--------------|-------------------------|
| rvv→zvbc_BD | +Zvbc BM + Forney | `compute_elp_zvbc`: vclmul.vv/vx SAXPY + blend + neg-stride dot (SEW=64, LMUL=4); `compute_error_values_zvbc`: vclmul.vv inv-power dot + inner-k batch |
| zvbc_BD→zvbc | +Zvbc iterative FFT | `fft_iterative`: recursive→BFS, `beta_scale_zvbc` + `xor_broadcast_zvbc` + `gammas_sums_combine_zvbc` (SEW=64/16, LMUL=4) |

## Analysis

**HQC-128**: B+D: −39194 instret (−29.1% of baseline); C: −21774 instret (−22.8% of zvbc_BD); Total: ×1.83

**HQC-192**: B+D: −43951 instret (−26.0% of baseline); C: −35276 instret (−28.2% of zvbc_BD); Total: ×1.88

**HQC-256**: B+D: −160693 instret (−37.4% of baseline); C: −37076 instret (−13.8% of zvbc_BD); Total: ×1.85

## End-to-End: Scalar Reference to Zvbc Champion

For complete context, combining with the Tier 2 ablation (decode_rm_rvv.md):

| Level | ref (scalar) | rvv (Tier 2) | zvbc (Tier 3) | Total speedup |
|-------|-------------:|:------------:|:-------------:|:-------------:|
| HQC-128 | 1,052,692 | 134,737 (×7.81) | 73,769 (×14.27) | ×14.27 |
| HQC-192 | 1,430,377 | 168,915 (×8.47) | 89,688 (×15.95) | ×15.95 |
| HQC-256 | 3,066,751 | 429,339 (×7.14) | 231,570 (×13.24) | ×13.24 |

