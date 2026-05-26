# Evaluation Platform Comparison

This document compares the four available evaluation platforms for benchmarking
the HQC-RISC-V implementation variants.

---

## Extension Coverage

| Extension | Instructions Used | Spike | gem5 v25.1 | Marian RTL (Ara+Zvk) | BPI-F3 (SpacemiT K1) |
|-----------|-------------------|:-----:|:----------:|:---------------------:|:---------------------:|
| **Zbc** | `clmul`, `clmulh` | ✅ | ✅ | ✅ (CVA6 scalar) | ✅ |
| **Zbb** | `ror`, `andn` | ✅ | ✅ | ✅ (CVA6 scalar) | ✅ |
| **Zba** | `sh1add`/`sh2add`/`sh3add` | ✅ | ✅ | ✅ (CVA6 scalar) | ✅ |
| **Zbs** | `bext`, `bset` | ✅ | ✅ | ⚠️ Verify CVA6 config | ✅ |
| **V** (RVV 1.0) | `vle64`, `vse64`, `vxor.vv`, `vsetvl`, etc. | ✅ | ✅ | ✅ (Ara v3.0) | ✅ (VLEN=256) |
| **Zvbc** | `vclmul.vv`, `vclmulh.vv` | ✅ | ✅ | ✅ (Zvk) | ❌ Not supported |
| **Zvbb** | `vror.vv`, `vandn.vv`, `vwsll` | ✅ | ✅ | ✅ (Zvk) | ❌ Not supported |

> **BPI-F3 Zvbc/Zvbb status:** The SpacemiT K1 X60 core does **not** implement
> Zvbc or Zvbb. Only scalar Zbc (`clmul`/`clmulh`) and base RVV 1.0 are present.
>
> **Consequence:** The BPI-F3 can only benchmark:
> - `ref_scalar`, `ref_vector`, `karatsuba_zbc`, `karatsuba_rvv` ✅
> - `karatsuba_zvbc` (needs `vclmul.vv`) ❌
> - Keccak `zvbb_x2_soa` (needs `vror.vv`, `vandn.vv`) ❌
>
> **Marian RTL is the only cycle-accurate platform** for the full vector-crypto
> variants.

Spike, gem5, and Marian RTL support the full extension set for all variants.
The BPI-F3 supports scalar and base-RVV variants only.

---

## Head-to-Head Comparison

| Criterion | Spike ISS | gem5 v25.1 | Marian RTL (Verilator) | BPI-F3 (K1 real silicon) |
|-----------|-----------|------------|------------------------|--------------------------|
| **What it measures** | Retired instruction count (`minstret`); `mcycle` = `minstret` (no pipeline model) | Simulated cycles with configurable pipeline, caches, branch prediction | Cycle-exact waveform for the Ara+CVA6 vector-crypto core | Real hardware cycles on SpacemiT K1 dual-issue in-order core |
| **Accuracy** | ISA-level only — no microarchitectural effects | Approximate — depends on model configuration fidelity | Cycle-exact for this specific RTL design | Ground truth for K1 silicon |
| **Microarchitecture modelled** | None (ideal single-issue, 1 CPI) | Configurable: in-order, OoO, pipeline depth, cache hierarchy | Fixed: CVA6 (single-issue, in-order scalar) + Ara (4-lane vector unit), VLEN=512 | Fixed: X60 core (dual-issue, in-order), VLEN=256 |
| **VLEN** | Configurable (256 in our scripts) | Configurable | 512 (Ara default) | 256 |
| **Pipeline effects** | ❌ | ✅ Stalls, hazards, issue width | ✅ Exact pipeline timing | ✅ Real pipeline |
| **Cache effects** | ❌ (optional `--dc` flag for D$ model) | ✅ Full L1/L2 hierarchy | ✅ Exact memory latency | ✅ 32KB L1I/D, 256KB L2 |
| **Vector unit throughput** | ❌ (1 instruction = 1 count) | ✅ Modelled (configurable latency) | ✅ Exact: multi-cycle `vclmul`, lane occupancy, chaining | ✅ Real throughput |
| **Determinism** | Perfectly deterministic | Deterministic for same config | Deterministic (RTL simulation) | Non-deterministic (OS jitter, interrupts, cache state) |
| **Setup time** | ~0 (already integrated) | ~1–2 days (build gem5, write config, validate) | ~1–3 days (clone Ara/Marian, resolve IPs with `bender`, build Verilator model, adapt linker/startup) | ~1 day (flash Bianbu, cross-compile Linux-ABI binaries, validate) |
| **Simulation speed** | ~50 MIPS | ~200 KIPS (TimingSimple) to ~50 KIPS (O3) | ~1–10 KIPS (Verilator) | Real-time (1.6 GHz) |
| **Time per HQC-128 polymul** (~274K instret) | ~5 ms | ~1–5 seconds | ~30–60 seconds | < 1 ms |
| **Time per full KEM** (~3M instret) | ~60 ms | ~15–60 seconds | ~5–10 minutes | < 5 ms |
| **Time for full suite** (5 variants × 3 levels × 3 ops) | < 10 seconds | ~30–90 minutes | ~4–12 hours | < 5 seconds (+ statistical repetitions) |
| **Publication strength** | Weak — no published PQC paper uses Spike instret as sole metric | Acceptable — TCHES 2025 NTT-RVV paper used gem5 for vector crypto | Strong — Marian published at ACM CCS 2024; cycle-exact RTL is gold standard for unavailable silicon | Strong for scalar/RVV variants (real silicon); **cannot benchmark Zvbc/Zvbb variants** (not supported) |
| **Key citation** | — | Barbosa et al., "Accelerating NTT with RISC-V Vector Extension for FHE", TCHES 2025 | Stregisters et al., "An Open Source RISC-V Processor with Zvk Vector Cryptography Extensions", ACM CCS 2024 (ePrint 2024/1449) | Zhang et al., "Optimized Software Implementation of Keccak, Kyber, and Dilithium on RV{32,64}IM{B}{V}", TCHES 2025 (used CanMV-K230 / C908 — different board, same methodology) |
| **Limitations** | No timing information; cannot distinguish fast vs slow instructions; not publishable alone | Timing is model-dependent — no validation target; vector unit latency assumptions may be inaccurate | Slow; VLEN=512 ≠ our target VLEN=256; single-issue scalar core ≠ K1's dual-issue; requires QuestaSim licence or Verilator build | **Zvbc/Zvbb not supported** — cannot benchmark `karatsuba_zvbc` or Keccak `zvbb_x2_soa`; OS noise requires many repetitions; dual-issue effects differ from single-issue Ara model |
| **What it proves** | Algorithmic instruction-count efficiency | Relative cycle performance under a plausible microarchitecture | Exact cycle cost on a real vector-crypto RTL design | Real-world performance on commercially available RISC-V silicon |

---

## Recommendation

### Primary evaluation: **Marian RTL (cycle-exact)**

- Cycle-exact for a real vector-crypto core with full Zvk (including Zvbc + Zvbb)
- Published at ACM CCS 2024 — strong citation
- Deterministic (no OS noise)
- Covers ALL variants including `karatsuba_zvbc` and Keccak `zvbb_x2_soa`
- Different microarchitecture (single-issue scalar + 4-lane Ara, VLEN=512) but still valid for relative comparisons

### Secondary evaluation: **BPI-F3 (real silicon — scalar/RVV variants only)**

- Real hardware cycles for `ref_scalar`, `ref_vector`, `karatsuba_zbc`, `karatsuba_rvv`
- Does NOT support `karatsuba_zvbc` or Keccak `zvbb_x2_soa` (no Zvbc/Zvbb)
- Fastest turnaround (real-time execution)
- Use statistical repetitions (N ≥ 100) to handle OS jitter
- Provides real IPC, cache miss rates via `perf stat`

### Development / fast iteration: **Spike**

- Keep as the fast inner-loop tool during optimisation
- `minstret` is useful for tracking instruction-count regressions
- Deterministic, instant, already integrated in CI

### Optional / supplementary: **gem5**

- Useful if you want to explore "what-if" microarchitectural scenarios (e.g., OoO core, different cache sizes, different vector unit widths)
- Weaker than RTL or real silicon but still publishable with proper justification
- Consider adding if time permits, but not essential given the other two options

---

## Suggested Thesis Evaluation Strategy

```
┌─────────────────────────────────────────────────────────────────┐
│  Development loop (daily)                                       │
│  ┌─────────┐                                                    │
│  │  Spike  │  instret counts, KAT validation, fast profiling   │
│  └─────────┘                                                    │
├─────────────────────────────────────────────────────────────────┤
│  Final evaluation (thesis results)                              │
│  ┌──────────────┐    ┌──────────────────┐                      │
│  │  BPI-F3 (K1) │    │  Marian RTL      │                      │
│  │  real cycles  │    │  cycle-exact     │                      │
│  │  VLEN=256     │    │  VLEN=512        │                      │
│  │  dual-issue   │    │  single-issue    │                      │
│  └──────────────┘    └──────────────────┘                      │
│         │                      │                                │
│         └──────┬───────────────┘                                │
│                ▼                                                 │
│  Cross-validate: do relative rankings agree?                    │
│  If yes → strong evidence that results generalise              │
│  If no  → interesting discussion on microarch sensitivity       │
├─────────────────────────────────────────────────────────────────┤
│  Optional: gem5 (configurable microarch exploration)            │
└─────────────────────────────────────────────────────────────────┘
```

---

## Cost Summary

| Platform | Hardware Cost | Software Cost | Licence Required |
|----------|-------------|---------------|-----------------|
| Spike | Free | Free (open-source) | None |
| gem5 | Free | Free (open-source) | None |
| Marian RTL + Verilator | Free | Free (open-source) | None |
| Marian RTL + QuestaSim | Free | Commercial | QuestaSim licence (university) |
| BPI-F3 | ~€50–80 (board + PSU + SD card) | Free (Bianbu Linux) | None |
