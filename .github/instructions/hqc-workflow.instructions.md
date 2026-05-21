---
description: "Use when working on HQC RISC-V optimizations, benchmarks, or infrastructure. Covers experimental isolation, promotion workflows, testing scope, trunk-based development, naming conventions, and Conventional Commits."
applyTo: ["**/*.c", "**/*.py", "**/*.sh", "**/*.md", "CMakeLists.txt", "experiments/**"]
name: "HQC RISC-V Co-Design Workflow Protocol"
---

# HQC RISC-V Co-Design Workflow Protocol

## 1. Experimental Isolation (Optimizations Only)

All performance optimizations, algorithm variants, and mathematical tweaks **MUST** be created as experiments. Bug fixes and refactors may be applied directly to `src/`.

### Create an Experiment

1. Create a new folder in `experiments/` following the naming pattern:
   ```
   experiments/EXP-<feature-name>/
   ```
   Example: `experiments/EXP-fafft-vclmul-butterfly/`, `experiments/EXP-polymul-schoolbook-unroll/`

2. Log the experiment in `experiments/manifest.yaml`:
   ```yaml
   experiments:
     - id: EXP-fafft-vclmul-butterfly
       status: in-progress
       description: "Vectorize FFT butterfly using vclmul.vv for carry-less multiplication"
       author: name
       started: 2026-05-05
       target_levels: [HQC-128, HQC-192, HQC-256]
   ```

3. Do not pollute `src/` with experimental versions. Use separate source files or CMake conditionals within `experiments/`.

### Never Link Experiments Into Default Targets

- Experimental code must not appear in the default stable build (i.e., `cmake --build build` must compile cleanly without experiments).
- Use `add_subdirectory(experiments/<exp-name>)` only when explicitly requested or in dedicated experiment builds.

---

## 2. Promotion: Proven Experiments to Stable

When an experiment passes all validation checks, promote it to replace the baseline implementation.

### Prerequisites for Promotion

- ✓ All Known Answer Tests (KATs) pass
- ✓ Benchmarked across **all three** security levels:
  - HQC-128 (n=512, k=256)
  - HQC-192 (n=768, k=384)
  - HQC-256 (n=1024, k=512)
- ✓ Cycle count / throughput improvements logged
- ✓ No side-channel leaks detected (timing, power, branch behavior)
- ✓ Code review approved

### Promotion Steps

1. **Move code** from `experiments/EXP-<name>/` to `src/poly_mul/scalar/` or `src/poly_mul/vector/`.
   - Rename files if necessary (e.g., `fafft_vclmul.c` → replaces old variant).
   - Update `#include` paths and CMake references.

2. **Update CMakeLists.txt** to remove experiment conditionals and link the new stable code.

3. **Mark experiment as promoted** in `experiments/manifest.yaml`:
   ```yaml
   experiments:
     - id: EXP-fafft-vclmul-butterfly
       status: promoted_to_stable
       promoted_date: 2026-05-10
       performance_delta_percent: +12.3
       promoted_to: src/poly_mul/vector/fafft_vclmul.c
   ```

4. **Verify the build**:
   ```bash
   cmake --build build
   ctest --build-config Debug --output-on-failure
   ```

5. **Commit with semantic versioning** (see Convention Commits section below).

---

## 3. Comprehensive Testing Scope

Do not assume an optimization scales linearly. Validate thoroughly across all security levels.

### Known Answer Tests (KATs)

- Located in `tests/kat/`
- Run before each merge:
  ```bash
  cmake --build build --target run_kat_tests
  ```
- All KATs must pass without modification.

### Benchmarking Requirements

- Benchmark implementations across HQC-128, HQC-192, HQC-256.
- Log cycle counts and timing data in `analysis/benchmarks/`.
- Update `analysis/benchmarks/cycle_counts.csv` and `analysis/benchmarks/instruction_mix.csv` with new measurements.
- Include LLVM MCA reports (`analysis/llvm_mca/reports/`) for cycle prediction validation.
- Compare baseline vs. optimized side-by-side in performance reports.

### Constant-Time Verification

- For operations on secret data (decoding, error correction), use constant-time verification:
  - `tests/ct/dudect/` — DudeCT statistical tests
  - `tests/ct/ctgrind/` — ctgrind timing simulation
- Any timing variance must be documented and justified.

---

## 4. Trunk-Based Development (Main Branch Only)

All development occurs on the `main` branch. No feature branches.

### Strict Isolation Requirement

- Because there are no feature branches, **experimental code MUST be confined to `experiments/`**.
- Experimental code must never leak into the default stable build.
- Any developer pulling `main` should have a clean, working build by default.

### Pre-Commit Build Verification

Before committing to `main`:
```bash
cd build && cmake .. && make -j$(nproc)
```

Ensure `cmake --build build` compiles cleanly **without** experimental features enabled.

---

## 5. File Naming Conventions

### C/C++/Assembly Files

- Use **`snake_case`** for all filenames (e.g., `poly_mul.c`, `fafft_scalar.c`).
- Use **explicit suffixes** to denote hardware-specific implementations:
  - `_rvv.c` — RISC-V Vector extension (RVV)
  - `_zbc.c` — Carry-less multiplication extension (Zbc)
  - `_zvc.c` — Carryless multiply for cryptography (Zvc)
  - `_scalar.c` — Portable C implementation
  - `_hybrid.c` — Mixed scalar + vector approach
  
  Examples:
  - `fafft_rvv.c` — Fast FFT using RVV
  - `polymul_zbc.c` — Polynomial multiplication using Zbc
  - `keccak_hybrid.c` — Keccak mixing scalar and vector operations

### Python and Shell Scripts

- Include file extension: `.py` for Python, `.sh` for shell scripts.
- Use **`snake_case`** for script names (e.g., `run_benchmarks.py`, `build_toolchain.sh`).
- Scripts must be marked as executable (`chmod +x`).
- Place scripts logically within `scripts/` subdirectories:
  - `scripts/bench/` — Benchmarking scripts
  - `scripts/ci/` — CI/CD automation
  - `scripts/debug/` — Debugging utilities
  - `scripts/report/` — Report generation

---

## 6. Conventional Commits Semantic

All git commit messages follow [Conventional Commits](https://www.conventionalcommits.org/) to enable automated changelog generation and performance tracking.

### Commit Message Format

```
<type>(<scope>): <subject>

<body (optional)>

<footer (optional)>
```

### Types

| Type | Use | Example |
|------|-----|---------|
| `feat` | New feature or algorithm variant | `feat(polymul): add schoolbook multiplication variant` |
| `fix` | Bug fix | `fix(decoder): correct error correction logic` |
| `perf` | Performance optimization | `perf(vector): optimize fafft butterfly loop using vclmul.vv` |
| `refactor` | Code restructuring (no behavior change) | `refactor(src): extract common FFT utilities` |
| `chore` | Build, CI, tooling | `chore: update CMake to 3.25` |
| `docs` | Documentation | `docs: add RVV implementation guide` |
| `test` | Test additions or fixes | `test: add comprehensive KAT coverage for HQC-256` |

### Scopes

- `poly_mul` — Polynomial multiplication implementations
- `fafft` — Fast FFT algorithms
- `decoder` — Decoding and error correction
- `vector` — RISC-V Vector extension code
- `scalar` — Portable C implementations
- `benchmark` — Performance measurement
- `ci` — Continuous integration
- `build` — Build system

### Examples

```
perf(vector): vectorize error correction using vclmul.vv
- Reduces cycle count by 12% on HQC-256
- Tested across HQC-128, HQC-192, HQC-256

Closes #45, #46
```

```
feat(polymul): implement hybrid schoolbook-karatsuba
- Uses schoolbook for small degrees, switches to Karatsuba above threshold
- Performance delta: +8.5% on HQC-192

Benchmark: analysis/benchmarks/cycle_counts.csv
```

```
fix(decoder): ensure constant-time error correction on secret data
- Adds ctgrind verification
- Tests: tests/ct/ctgrind/
```

---

## Quick Checklist: Before Merging to Main

- [ ] Code follows file naming conventions (`snake_case`, hardware suffixes)
- [ ] Experiments isolated in `experiments/EXP-<name>/`, logged in `manifest.yaml`
- [ ] If promoting: all KATs pass, benchmarked across HQC-128/192/256
- [ ] If promoting: constant-time analysis complete
- [ ] `cmake --build build` compiles cleanly
- [ ] Commit message follows Conventional Commits
- [ ] No experimental code linked into default build targets
- [ ] Changes reflect performance/security milestones in commit message

---

## References

- **Experiments**: `experiments/manifest.yaml`
- **Benchmarks**: `analysis/benchmarks/`
- **Tests**: `tests/kat/`, `tests/ct/`
- **Reports**: `analysis/report/`, `docs/`
- **CMake**: `CMakeLists.txt`, `cmake/`

---

## 7. Architectural Tracks & Pairings

Every multiplier variant belongs to exactly one **track**. Tracks define which Keccak and Decode implementations must be co-selected. Future agents **MUST** use `bench_parse_track_args` (defined in `scripts/bench/lib_bench.sh`) as the single source of truth for CLI parsing and auto-mapping. Never hardcode Keccak or Decode impl choices independently of the multiplier suffix.

### Track Definitions

| Track | Multiplier Suffix(es) | Keccak Impl | Decode Impl | Notes |
|-------|----------------------|-------------|-------------|-------|
| **A — Scalar/Zbc** | `_ref`, `_zbc`, `hybrid` | `ref` | `ref` | Pure scalar; Zbc clmul; no RVV |
| **B — Base RVV** | `_rvv` | `rvv_only` | `rvv` | Standard RVV 1.0 vector extension |
| **C — Vector Crypto** | `_zvbc` | `zvbb_x2_soa` | `rvv` | Zvbc vclmul.vv + Zvbb x2 SoA |

### Auto-Mapping Rules (implemented in `bench_parse_track_args`)

```bash
case "$MUL_VARIANT" in
    *_zvbc)             KECCAK_IMPL="zvbb_x2_soa" ;;  # Track C
    *_rvv)              KECCAK_IMPL="rvv_only"    ;;  # Track B
    *_zbc|*_ref|hybrid) KECCAK_IMPL="ref"         ;;  # Track A
    *)                  KECCAK_IMPL="ref"          ;;  # Safe default
esac
case "$MUL_VARIANT" in
    *_zvbc|*_rvv)       DECODE_IMPL="rvv" ;;
    *_zbc|*_ref|hybrid) DECODE_IMPL="ref" ;;
    *)                  DECODE_IMPL="ref" ;;
esac
```

### Strict Adherence Rules for Agents

1. **Naming:** All new multiplier source files and variant names MUST carry the correct track suffix (`_rvv`, `_zvbc`, `_zbc`, `_ref`, `hybrid`). Never create a variant without a suffix — ambiguity breaks auto-mapping.

2. **CMake pairing:** When adding a new variant to `CMakeLists.txt` or `cmake/options.cmake`, assign it to the correct `VECTOR_POLYMUL_VARIANTS` list (Track B/C) or leave it out of that list (Track A).

3. **CLI usage:** All benchmark, profile, KAT, and analysis scripts accept `--mul <variant>`. Never pass `--variant`. The `--keccak` and `--decode` flags override auto-mapping and should only be used for explicit experiments.

4. **No cross-track mixing:** Do not combine a Track A multiplier with `rvv_only` Keccak, or a Track B multiplier with `zvbb_x2_soa` Keccak, unless running an intentional cross-track experiment documented in `experiments/manifest.yaml`.

5. **Reconfiguration detection:** `bench_auto_configure_and_build` reads `CMakeCache.txt` to detect Keccak/Decode impl changes and forces full CMake reconfiguration when a mismatch is found. Do not bypass this by manually editing cached build dirs.
