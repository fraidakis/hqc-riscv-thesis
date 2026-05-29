# RISC-V Processors with Zvbc & Zvbb Vector Cryptography Extensions

> **Purpose:** Identify cycle-accurate RTL targets or accessible hardware for benchmarking
> HQC (Post-Quantum Cryptography) on RISC-V with vector carry-less multiplication (Zvbc)
> and vector basic bit-manipulation (Zvbb).
>
> **Last Updated:** 2026-05-28

---

## Executive Summary

The RISC-V Vector Cryptography Extensions (Zvk) were ratified in 2023 and merged into
the main ISA manuals in 2024. The Zvk umbrella includes Zvbb (vector bit-manipulation
for crypto) and Zvbc (vector carry-less multiplication), both critical for GF(2^m)
polynomial arithmetic in HQC.

As of mid-2026, the landscape for **cycle-accurate RTL** with explicit Zvbc+Zvbb support
is extremely narrow. Only one open-source processor has been published with full Zvk
hardware support. Commercial IP cores (SiFive P870, Tenstorrent Ascalon) claim RVA23
compliance which mandates Zvkb (a subset of Zvbb) but their RTL is not publicly
available.

---

## 1. Marian — SoC Hub Finland (Tampere University)

| Field | Details |
|-------|---------|
| **Developer** | SoC Hub, Tampere University, Finland |
| **Type** | Open-Source RTL (SystemVerilog) |
| **Base** | Fork of PULP Ara v3.0 (CVA6 scalar + Ara vector unit) |
| **Extension Support** | Full Zvk (includes Zvbb, Zvbc, Zvkg, Zvkned, Zvknha, Zvknhb, Zvksed, Zvksh) + RVV 1.0 |
| **VLEN** | Configurable; Ara default is 4096 bits (4 lanes × 64-bit × 16 elements) |
| **ELEN** | 64-bit |
| **Repository** | [github.com/soc-hub-fi/Marian](https://github.com/soc-hub-fi/Marian) |
| **Paper** | "An Open Source RISC-V Processor with Zvk Vector Cryptography Extensions," ACM CCS 2024 ([IACR ePrint 2024/1449](https://eprint.iacr.org/2024/1449)) |
| **License** | Solderpad Hardware License (permissive, academic-friendly) |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (RVV 1.0) | ✅ | Full 64-bit vector, inherited from Ara v3.0 |
| Zvbb | ✅ | vbrev, vclz, vctz, vcpop, vwsll, vrol, vror, vandn, vrev8, vbrev8 |
| Zvbc | ✅ | vclmul.vv, vclmul.vx, vclmulh.vv, vclmulh.vx |
| Zvkg | ✅ | vgmul, vghsh (GCM/GMAC) |
| Zvkned | ✅ | AES encrypt/decrypt |
| Zvknhb | ✅ | SHA-512/SHA-256 |
| Zvksed | ✅ | SM4 |
| Zvksh | ✅ | SM3 |

### Toolchain Requirements

- **GCC ≥ 13.2** with `-march=rv64gcv_zvk` (or individual sub-extensions)
- **LLVM/Clang ≥ 17** supports Zvk intrinsics
- Custom linker scripts for bare-metal (provided in `sw/` directory)
- RISC-V OpenOCD for FPGA debug (tested with commit 3991492)

### Simulation & FPGA Support

| Tool | Version | Status |
|------|---------|--------|
| Questasim | 10.7g | Primary verification flow |
| Verilator | v5.008 | Supported (cycle-accurate) |
| Vivado | 2021.2 | FPGA prototyping (Virtex UltraScale+) |
| VCS | — | Not documented but likely feasible |

### Deployment Effort Assessment

**Difficulty: Medium**

- Repository is well-structured with Makefile-driven flows
- `make repository_init` pulls all submodules
- Verilator flow: `make verilate && make simv TEST=<your_test>`
- Questasim flow: `make compile elaborate && make simc TEST=<your_test>`
- Software hex files loaded via `$readmemh` — straightforward bare-metal integration
- FPGA flow documented for Xilinx Virtex UltraScale+ (VCU118-class boards)
- **Key concern:** Repository has only 1 contributor and last commit was ~2 years ago.
  May require some maintenance effort to integrate with latest toolchains.

### Relevance for HQC

**HIGH** — This is the strongest candidate:
- VLEN=4096 provides wide vectors ideal for GF(2^m) polynomial multiplication
- Zvbc gives native `vclmul` for carry-less multiplication (core HQC operation)
- Zvbb provides bit-reversal and rotation needed for NTT/FFT-like operations
- Cycle-accurate simulation via Verilator enables precise benchmarking
- Open-source allows inspection of pipeline behavior and microarchitectural tuning

---

## 2. CHIPS Alliance T1 (Torrent-1)

| Field | Details |
|-------|---------|
| **Developer** | CHIPS Alliance (Jiuyang Liu et al.) |
| **Type** | Open-Source RTL (Chisel/Scala → SystemVerilog) |
| **Base** | Custom lane-based vector unit + forked Rocket scalar core |
| **Extension Support** | Zve32x, Zve32f; **Zvbb partially implemented** (LaneZvbb module exists) |
| **VLEN** | Configurable up to 64K bits |
| **ELEN** | 32-bit (Zve32x — no 64-bit element support) |
| **Repository** | [github.com/chipsalliance/t1](https://github.com/chipsalliance/t1) |
| **License** | Apache-2.0 |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (full RVV 1.0) | ❌ | Only Zve32x/Zve32f (32-bit elements max) |
| Zvbb | ⚠️ Partial | `LaneZvbb` module exists in source tree; recently migrated to zaozi |
| Zvbc | ❌ | No evidence of vclmul implementation (requires 64-bit elements) |
| Zvkg | ❌ | Not documented |

### Deployment Effort Assessment

**Difficulty: High (and likely unsuitable)**

- Nix-based build system (complex but reproducible)
- Verilator and VCS emulators available via `nix build`
- Docker images available for quick start
- **Critical limitation:** ELEN=32 means Zvbc (which operates on 64-bit elements for
  carry-less multiplication) cannot be supported. This makes T1 **unsuitable for HQC
  benchmarking** that relies on 64-bit vclmul operations.

### Relevance for HQC

**LOW** — The 32-bit element width fundamentally prevents Zvbc support. While the
massive VLEN is attractive, without 64-bit carry-less multiplication the core cannot
execute the critical HQC polynomial multiplication kernels.

---

## 3. SiFive Performance P870 / P570 (Commercial IP)

| Field | Details |
|-------|---------|
| **Developer** | SiFive, Inc. |
| **Type** | Commercial IP (RTL not publicly available) |
| **Base** | 6-wide out-of-order 64-bit core |
| **Extension Support** | RVA23 profile compliant; Vector Crypto extensions confirmed |
| **VLEN** | 2×128-bit (256-bit effective) |
| **Repository** | N/A (proprietary) |
| **Product Page** | [sifive.com/cores/performance-p800](https://www.sifive.com/cores/performance-p800) |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (RVV 1.0) | ✅ | Full RVV 1.0 with 2×128b VLEN |
| Zvbb | ✅ (likely) | RVA23 mandates Zvkb (subset); full Zvbb probable given "vector crypto" claim |
| Zvbc | ✅ (likely) | Part of Zvk bundle; P870 advertises "vector crypto" |
| Full Zvk | ✅ | Explicitly stated in marketing materials |

### Toolchain Requirements

- Mainline GCC ≥ 14 / LLVM ≥ 17 with `-march=rv64gcv_zvk`
- SiFive provides their own SDK for licensees

### Deployment Effort Assessment

**Difficulty: Not Feasible for Academic Use**

- RTL is proprietary and requires commercial licensing
- No public FPGA bitstreams or evaluation boards with P870 silicon available yet
- SiFive HiFive boards (existing) use older cores without Zvk
- Academic licensing may be possible but would require NDA and significant lead time

### Relevance for HQC

**MEDIUM (theoretical)** — The VLEN=256 is relatively narrow for HQC workloads.
While Zvbc is supported, the limited vector width means more strip-mining iterations.
More critically, the RTL is inaccessible for cycle-accurate academic benchmarking.

---

## 4. Tenstorrent Ascalon (Commercial IP)

| Field | Details |
|-------|---------|
| **Developer** | Tenstorrent (Jim Keller's team) |
| **Type** | Commercial IP (RTL not publicly available) |
| **Base** | High-performance out-of-order RISC-V core |
| **Extension Support** | RVA23 profile compliant; RVV 1.0 + Vector Crypto |
| **VLEN** | Not publicly documented |
| **Product Page** | [tenstorrent.com/ip/risc-v-cpu](https://tenstorrent.com/ip/risc-v-cpu) |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (RVV 1.0) | ✅ | Confirmed via RVA23 compliance |
| Zvbb | ✅ (inferred) | RVA23 mandates Zvkb; full Zvbb likely |
| Zvbc | ✅ (inferred) | Part of Zvk/Zvks bundles in RVA23 optional set |
| Full Zvk | Likely | "Vector crypto" mentioned in announcements |

### Deployment Effort Assessment

**Difficulty: Not Feasible**

- Proprietary IP, no public RTL
- No silicon available for academic access
- Tenstorrent's accessible hardware (Wormhole/Blackhole cards) uses different
  RISC-V cores (Tensix) without standard vector extensions

### Relevance for HQC

**NONE (inaccessible)** — Cannot obtain RTL or hardware for benchmarking.

---

## 5. SpacemiT K1 / X60 (Silicon — Accessible Hardware)

| Field | Details |
|-------|---------|
| **Developer** | SpacemiT |
| **Type** | Silicon (available on commercial SBCs) |
| **Base** | Octa-core X60, 8-stage dual-issue in-order pipeline |
| **Extension Support** | RVV 1.0 (VLEN=256); Scalar crypto (Zk); **Zvbc/Zvbb status unconfirmed** |
| **VLEN** | 256-bit |
| **Available Boards** | Banana Pi BPI-F3, Milk-V Jupiter, Muse Pi Pro |
| **Testbed** | [ExCALIBUR RISC-V Testbed (EPCC Edinburgh)](http://riscv.epcc.ed.ac.uk/documentation/hardware/) |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (RVV 1.0) | ✅ | 256-bit VLEN confirmed |
| Zvbb | ❓ Unconfirmed | Not explicitly listed in public datasheets |
| Zvbc | ❓ Unconfirmed | Not explicitly listed; K1 targets RVA22 (not RVA23) |
| Scalar Zk | ✅ | Zbkb, Zbkc, Zbkx confirmed |

### Deployment Effort Assessment

**Difficulty: Low (if extensions are present)**

- Boards available for ~$100 (Banana Pi BPI-F3, Milk-V Jupiter)
- Linux runs natively; bare-metal possible with vendor U-Boot
- EPCC Edinburgh testbed offers remote access
- **Critical uncertainty:** The X60 core targets RVA22 compliance (not RVA23).
  RVA22 does NOT mandate vector crypto extensions. Public documentation does not
  explicitly confirm Zvbc or Zvbb support. The chip may only have scalar crypto (Zk).
- Would need to check `/proc/cpuinfo` or use `hwprobe` syscall on actual hardware
  to confirm extension presence.

### Relevance for HQC

**LOW-MEDIUM (uncertain)** — Even if Zvbc is present, VLEN=256 is narrow, and the
in-order dual-issue pipeline gives limited IPC. More critically, this is real silicon
without RTL access, so you cannot get true cycle-accurate pipeline visibility. You
would get wall-clock timing and perf counters, but not the microarchitectural detail
available from RTL simulation.

---

## 6. Saturn Vector Unit (UC Berkeley)

| Field | Details |
|-------|---------|
| **Developer** | UC Berkeley (BAR group) |
| **Type** | Open-Source RTL (Chisel) |
| **Base** | Parameterized vector unit for Chipyard/Rocket/BOOM integration |
| **Extension Support** | RVV 1.0; **No Zvk/Zvbc/Zvbb support documented** |
| **VLEN** | Configurable |
| **Repository** | [github.com/ucb-bar/saturn-vectors](https://github.com/ucb-bar/saturn-vectors) |
| **Manual** | [saturn-vectors.org](https://www.saturn-vectors.org/) |
| **Paper** | "The Saturn Microarchitecture Manual," UC Berkeley EECS-2024-215 |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (RVV 1.0) | ✅ | Full compliance |
| Zvbb | ❌ | Not mentioned in documentation or manual |
| Zvbc | ❌ | Not mentioned |
| Zvk | ❌ | No crypto extensions implemented |

### Deployment Effort Assessment

**Difficulty: Medium (but no Zvk)**

- Well-documented Chipyard integration
- Verilator simulation straightforward
- Extensible architecture designed for adding custom functional units
- **Could potentially be extended** to add Zvbc/Zvbb, but this would be a significant
  hardware design effort (weeks to months)

### Relevance for HQC

**NONE (currently)** — No vector crypto support. Would require implementing Zvbc/Zvbb
functional units from scratch. Potentially interesting as a future extension project
but not viable for thesis timeline.

---

## 7. Andes AX45MPV (Commercial IP)

| Field | Details |
|-------|---------|
| **Developer** | Andes Technology |
| **Type** | Commercial IP |
| **Base** | 64-bit multicore with up to 1024-bit VPU |
| **Extension Support** | RVV 1.0; **Zvbc/Zvbb status unclear** |
| **VLEN** | Up to 1024-bit |
| **Product Page** | [andes.com](https://www.andestech.com/) |

### Extension Support Matrix

| Extension | Supported | Notes |
|-----------|-----------|-------|
| V (RVV 1.0) | ✅ | Up to 1024-bit VLEN |
| Zvbb | ❓ | Not explicitly confirmed in public materials |
| Zvbc | ❓ | Not explicitly confirmed |

### Deployment Effort Assessment

**Difficulty: Not Feasible**

- Proprietary IP requiring commercial license
- Andes Voyager board (QiLai SoC) uses AX45MP + NX27V but crypto extensions unconfirmed
- No academic evaluation program publicly documented for vector crypto

### Relevance for HQC

**NONE (inaccessible and unconfirmed)**

---

## Comparative Summary

| Target | Zvbc | Zvbb | VLEN | RTL Access | Cycle-Accurate | Effort | **Recommendation** |
|--------|------|------|------|------------|----------------|--------|-------------------|
| **Marian** | ✅ | ✅ | 4096 | ✅ Open | ✅ Verilator/Questa | Medium | **⭐ PRIMARY TARGET** |
| T1 | ❌ | ⚠️ | 64K | ✅ Open | ✅ Verilator/VCS | High | ❌ ELEN=32 blocks Zvbc |
| SiFive P870 | ✅ | ✅ | 256 | ❌ Proprietary | N/A | N/A | ❌ Inaccessible |
| Tenstorrent Ascalon | ✅ | ✅ | ? | ❌ Proprietary | N/A | N/A | ❌ Inaccessible |
| SpacemiT K1 | ❓ | ❓ | 256 | ❌ Silicon only | ❌ Perf counters only | Low | ⚠️ Verify extensions first |
| Saturn | ❌ | ❌ | Config | ✅ Open | ✅ Verilator | High+ | ❌ No Zvk support |
| Andes AX45MPV | ❓ | ❓ | 1024 | ❌ Proprietary | N/A | N/A | ❌ Inaccessible |

---

## Recommendation for Thesis Work

### Primary Target: Marian

**Marian is the only viable option** for cycle-accurate RTL simulation with confirmed
Zvbc+Zvbb support and open-source access. Key advantages:

1. **VLEN=4096** — Wide vectors minimize strip-mining overhead for HQC's large
   polynomial operations
2. **Full Zvk** — Native `vclmul.vv`/`vclmul.vx` for GF(2^m) multiplication
3. **Verilator support** — Enables cycle-accurate profiling without commercial EDA tools
4. **FPGA-proven** — Tested on Virtex UltraScale+ for validation
5. **Academic pedigree** — Published at ACM CCS 2024, based on well-known Ara/CVA6

### Deployment Plan

```
1. Clone: git clone --recursive https://github.com/soc-hub-fi/Marian
2. Init:  make repository_init
3. Build: make verilate  (or make compile elaborate for Questa)
4. Test:  make simv TEST=hello_world
5. Custom: Compile HQC bare-metal binary → convert to .hex → run via make simv TEST=hqc
```

### Risks & Mitigations

| Risk | Mitigation |
|------|-----------|
| Repo inactive (2 years) | Fork and maintain; core RTL is stable |
| Verilator v5.008 specific | Pin version or test with newer releases |
| GCC 13.2 required | Use riscv-gnu-toolchain with Zvk enabled |
| No perf counters in RTL | Add CSR-based cycle/instret counters or use Verilator VCD traces |

### Alternative: Extend Your Existing Simulation

If Marian integration proves too complex, consider that your current project already
uses a simulation infrastructure. You could:
- Use Marian's Zvbc functional unit RTL as a reference
- Validate algorithmic correctness against your existing Spike/QEMU setup
- Extract cycle estimates from Marian's Verilator traces for the critical kernels only

---

## RVA23 Profile & Zvbc/Zvbb Status

For reference, the RVA23 profile (ratified October 2024) includes:
- **Mandatory:** Zvkb (subset of Zvbb — includes vandn, vbrev8, vrev8, vrol, vror, vclmul.vv/vx)
- **Optional (but recommended):** Zvbb (full — adds vbrev, vclz, vctz, vcpop, vwsll)
- **Optional:** Zvbc, Zvkg, Zvkned, Zvknhb, Zvksed, Zvksh (and bundle extensions Zvkn, Zvks)

Note: Zvkb includes `vclmul` (carry-less multiply) instructions, meaning **any RVA23-compliant
processor MUST support vclmul**. This is significant — it means Zvbc's carry-less multiply
high (`vclmulh`) is optional, but the base `vclmul` is mandatory via Zvkb.

---

## References

1. T. Szilvási et al., "An Open Source RISC-V Processor with Zvk Vector Cryptography
   Extensions," ACM CCS 2024. [ePrint](https://eprint.iacr.org/2024/1449)
2. CHIPS Alliance T1 Repository. [GitHub](https://github.com/chipsalliance/t1)
3. PULP Platform Ara. [GitHub](https://github.com/pulp-platform/ara)
4. SiFive P870 Product Page. [SiFive](https://www.sifive.com/cores/performance-p800)
5. RISC-V Crypto Extensions Specification. [GitHub](https://github.com/riscv/riscv-crypto)
6. RVA23 Profile Specification. [GitHub](https://github.com/riscv/riscv-profiles/blob/main/src/rva23-profile.adoc)
7. Saturn Vector Unit Manual. [saturn-vectors.org](https://www.saturn-vectors.org/)
8. ExCALIBUR RISC-V Testbed. [EPCC](http://riscv.epcc.ed.ac.uk/documentation/hardware/)

---

*Content was rephrased for compliance with licensing restrictions. Sources cited inline.*
