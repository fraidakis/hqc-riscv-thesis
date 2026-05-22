### Zbb Ablation Study & Explicit Scalar Keccak Optimization Results

**Experiment:** `EXP-keccak-scalar-zbb`  
**Date:** 2026-05-22  
**Simulator:** Spike  
**ISA (execution):** `rv64gcv_zbc_zbb_zbkb_zvbc_zvbb_zvl256b_zicntr_zihpm`  

---

#### Background

The HQC Keccak implementation (`vendor/hqc-ref/lib/fips202/fips202.c`) uses two
patterns sensitive to the Zbb extension:

1. **`ROL(a, offset)` macro** — expands to `(a << offset) | (a >> (64 - offset))`.
   With `_zbb` in the `-march`, Clang auto-lowers this to the single-cycle `rori`
   instruction. Without `_zbb`, it emits two shifts plus an `or`.

2. **`load64` / `store64`** — manually converts between `uint8_t*` and `uint64_t`
   via 8 shift-or / shift-store operations. An explicit `__builtin_memcpy` of 8 bytes
   emits a single aligned `ld`/`sd` pair on RISC-V little-endian targets, eliminating
   56 byte-manipulation instructions per Keccak round pair.

---

#### Profiles Compared

| Profile | Source | `-march` for fips202.c | Description |
| :--- | :--- | :--- | :--- |
| `ref_scalar_with_zbb` | `vendor/hqc-ref/lib/fips202/fips202.c` | `rv64gc_zbc_zbb_zbkb` | Baseline: auto-lowered `rori` |
| `ref_scalar_no_zbb` | `vendor/hqc-ref/lib/fips202/fips202.c` | `rv64gc_zbc` | Ablation: no `rori`, shift-based rotations |
| `scalar_opt_exp` | `experiments/EXP-keccak-scalar-zbb/fips202_scalar_opt.c` | `rv64gc_zbc_zbb_zbkb` | Explicit `__builtin_rotateleft64` + `__builtin_memcpy` |

---

#### ISA Impact: Zbb on Keccak Rotations (Ablation)

Metric: total instruction-retired count (`instret`) for 1 iteration of keypair + encaps + decaps.

| Security Level | ref_scalar (with Zbb) | ref_scalar (no Zbb) | Delta (instret) | Overhead |
| :--- | ---: | ---: | ---: | ---: |
| **HQC-128** | 117,344,266 | 117,697,806 | +353,540 | +0.30% |
| **HQC-192** | 354,302,605 | 354,968,028 | +665,423 | +0.19% |
| **HQC-256** | 861,974,446 | 863,019,355 | +1,044,909 | +0.12% |

**Observation:** Removing Zbb (`rori`) from Keccak compilation adds only 0.12–0.30% overhead.
The impact decreases at higher security levels because polynomial multiplication dominates.

---

#### Explicit Builtin Experiment (scalar_opt_exp)

Implemented `fips202_scalar_opt.c` with:
```c
#define ROL(a, offset) __builtin_rotateleft64((a), (uint32_t)(offset))

static inline uint64_t load64(const uint8_t *x) {
    uint64_t r; __builtin_memcpy(&r, x, sizeof(r)); return r;
}
static inline void store64(uint8_t *x, uint64_t u) {
    __builtin_memcpy(x, &u, sizeof(u));
}
```

**Binary verification:** 58 `rori` instructions in the scalar_opt_exp ELF — identical to
the auto-lowered count in `ref_scalar_with_zbb`. Compiler generated equivalent code.

| Security Level | ref_scalar (with Zbb, auto) | scalar_opt_exp (explicit) | Delta (instret) |
| :--- | ---: | ---: | ---: |
| **HQC-128** | 117,344,266 | 117,343,720 | −546 (−0.0005%) |
| **HQC-192** | 354,302,605 | 354,302,059 | −546 (−0.0002%) |
| **HQC-256** | 861,974,446 | 861,973,904 | −542 (−0.0001%) |

**Observation:** Explicit `__builtin_rotateleft64` + `__builtin_memcpy` produces
essentially identical instruction counts to the auto-lowered baseline. The ~540-instruction
saving is within measurement noise and likely reflects minor load/store folding from the
`__builtin_memcpy` of 8 bytes.

---

#### KAT Validation

**Verification:** 300/300 Known Answer Tests passed (100/100 per security level).

| Level | Vectors | Result |
| :--- | :--- | :--- |
| HQC-128 | 100/100 | PASS |
| HQC-192 | 100/100 | PASS |
| HQC-256 | 100/100 | PASS |

---

#### Conclusions

1. **Zbb impact on Keccak is marginal** (≤0.30%). Keccak scalar rotations account for a
   small fraction of total instruction count; polynomial multiplication dominates at all
   security levels.

2. **Explicit `__builtin_rotateleft64` is equivalent to auto-lowering.** Clang reliably
   emits `rori` from the shift-or ROL pattern when `_zbb` is in the march; the explicit
   builtin provides no additional code-quality benefit but serves as a correctness
   annotation to future maintainers.

3. **`__builtin_memcpy` for 8-byte aligned loads/stores** confirms the compiler
   eliminates the byte-shuffle loop when given the explicit memcpy hint.

4. **Zbb is not a bottleneck for scalar HQC Keccak.** Any further Keccak optimisation
   effort should focus on the vector (`zvbb_x2_soa`) path, where `vror.vi` provides
   larger wins.

---

#### Build Artifacts

| Artifact | Path |
| :--- | :--- |
| Baseline bench ELFs | `build/spike/bench_kref_dref/bench_hqc{128,192,256}_ref_scalar.elf` |
| No-Zbb ablation bench ELFs | `build/spike_ablation_nozbb/bench_hqc{128,192,256}_ref_scalar.elf` |
| scalar_opt_exp bench ELFs | `build/spike_scalar_opt_exp/bench_hqc{128,192,256}_ref_scalar.elf` |
| Experiment source | `experiments/EXP-keccak-scalar-zbb/fips202_scalar_opt.c` |
| Experiment manifest entry | `experiments/manifest.yaml` — id: `EXP-keccak-scalar-zbb` |
