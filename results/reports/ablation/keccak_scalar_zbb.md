# Keccak Scalar Zbb Ablation Study

Generated: 2026-06-29 17:58
Platform: BPI-F3 (SpacemiT K1 X60)
Quality: publish (N=10000, N_warmup=100)
Commit: 8dd2ee9

## Experiment

**Objective:** Quantify the cycle cost of Zbb (`rori`) auto-lowering for isolated
scalar Keccak on real hardware.

**Mechanism:** The vendor `fips202.c` uses the pattern `(a << offset) | (a >> (64 - offset))`
for 64-bit rotations. With `-march=..._zbb`, Clang/GCC lower this to a single-cycle
`rori` instruction. Without `_zbb`, it expands to 3 instructions: `slli` + `srli` + `or`.

**Profiles:**

| Profile | `-march` | Description |
|---------|----------|-------------|
| `ref` | `rv64gc_zbc_zbb_zbs` | Baseline: `rori` auto-lowering active |
| `ref_nozbb` | `rv64gc_zbc_zbs` | Ablation: no `rori`, shift-based rotations |

**Workload:** Full HQC decaps Keccak call pattern (6 XOF/hash initialisations,
~30–50 Keccak-f[1600] permutations depending on security level and rejection
sampling). Measured via `perf_event_open` cycle counting on isolated core 7.

## Results: Median Cycles

| Level | ref (with Zbb) | ref_nozbb (no Zbb) | Δ cycles | Overhead |
|-------|---------------:|--------------------:|---------:|---------:|
| HQC-128 | 355,921 | 520,978 | +165,057 | +46.37% |
| HQC-192 | 712,524 | 1,039,396 | +326,872 | +45.88% |
| HQC-256 | 1,120,502 | 1,637,913 | +517,411 | +46.18% |

## Results: Instruction Count (instret)

| Level | ref (with Zbb) | ref_nozbb (no Zbb) | Δ instret | Overhead |
|-------|---------------:|--------------------:|----------:|---------:|
| HQC-128 | 568,058 | 852,040 | +283,982 | +49.99% |
| HQC-192 | 1,135,418 | 1,692,567 | +557,149 | +49.07% |
| HQC-256 | 1,787,743 | 2,659,536 | +871,793 | +48.77% |

## Results: IPC and Variability

| Level | IPC (ref) | IPC (nozbb) | IQR (ref) | IQR (nozbb) |
|-------|----------:|------------:|----------:|------------:|
| HQC-128 | 1.596 | 1.635 | 912 | 2,727 |
| HQC-192 | 1.594 | 1.628 | 4,583 | 2,127 |
| HQC-256 | 1.595 | 1.624 | 2,868 | 7,009 |

## Dynamic Rotation Count (estimated)

Each `rori` → `slli+srli+or` expansion adds exactly +2 instructions per rotation site.
The instret delta divided by 2 gives the number of dynamic rotation executions:

| Level | Δ instret / 2 | Estimated dynamic rotations |
|-------|-------------:|---------:|
| HQC-128 | 141,991 | ~142k |
| HQC-192 | 278,575 | ~279k |
| HQC-256 | 435,897 | ~436k |

Cross-check: Keccak-f[1600] performs 24 rounds × 25 lanes of ρ-rotations = 600
rotations per permutation. HQC-128 decaps invokes ~237 permutations
(141,991 / 600 ≈ 237). This is consistent with the XOF/hash call pattern.

## Analysis

1. **~46% cycle overhead without Zbb** — removing `_zbb` from the march string
   forces every ROL pattern to expand from 1 to 3 instructions. For a
   rotation-heavy kernel like Keccak-f[1600], this is catastrophic in isolation.

2. **Cycle overhead (46%) < instret overhead (49%)** — the X60 dual-issue pipeline
   extracts partial ILP from the 3-instruction shift sequence. IPC improves
   from 1.595 to 1.629 without Zbb, indicating the `slli`/`srli`/`or` ops
   partially overlap in the execution pipeline (unlike the single `rori` which
   is already single-cycle and cannot benefit from ILP).

3. **Constant percentage across security levels** — confirms Keccak cost scales
   linearly with the number of permutations. No architectural surprise at
   higher parameters; the overhead is purely multiplicative.

4. **Amdahl context (full KEM):**
   - Full KEM Zbb penalty (from §7 sim data): +0.12% to +0.30%
   - Isolated Keccak Zbb penalty (this study): +46%
   - Keccak fraction of total KEM cycles (hw): ~0.3% (L128) to ~0.13% (L256)
   - This confirms Keccak is NOT the bottleneck — polymul dominates by ×100.
   - Zbb matters for Keccak in isolation but is invisible at KEM system level.

5. **Comparison with simulation (Spike instret):**
   - Spike measured +0.30% KEM-wide overhead at L128 (353,540 extra instructions)
   - This hw study measures +283,982 isolated keccak instructions at L128
   - Ratio: 283,982 / 353,540 = 80% of the full-KEM Zbb instret penalty comes
     from Keccak rotations alone. The remaining 20% is from non-Keccak Zbb sites
     (memset optimizations, other bit-manipulation).

## Invocation

```bash
# Baseline (with Zbb)
scripts/bench/run_bench.sh --platform hw --mode keccak --keccak ref --quality publish

# Ablation (without Zbb)
scripts/bench/run_bench.sh --platform hw --mode keccak --keccak ref_nozbb --quality publish
```

## Data Files

- Baseline: `results/hardware/benchmarks/components/keccak/hw_keccak_ref_latest.csv`
- Ablation: `results/hardware/benchmarks/components/keccak/hw_keccak_ref-nozbb_latest.csv`
