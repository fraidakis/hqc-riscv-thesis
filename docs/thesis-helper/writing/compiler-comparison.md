# Compiler & Simulator Toolchain Analysis for RISC-V HQC Acceleration

> **Document scope:** A technical reference for the Master's thesis *"Pure Software Acceleration of HQC on RISC-V."* Every recommendation below is anchored to the specific demands of compiling RVV intrinsic-heavy cryptographic C, generating correct `clmul`/`vclmul` instruction sequences, and running bare-metal KAT verification on ISA simulators.

---

## 1. Executive Summary

The thesis requires simultaneous support for **five RISC-V extensions** (`Zbc`, `V`, `Zbb`, `Zbkb`, `Zvbc`) across three distinct compilation modes: pure-C reference code, RVV-intrinsic cryptographic kernels, and hand-written RV64 inline assembly. No single pre-packaged toolchain satisfies all of these requirements out of the box.

**Core conclusions:**
- **GCC 14+ is the minimum viable compiler** for `Zvbc` (vector carry-less multiply) support. Ubuntu `apt` packages are categorically insufficient.
- **LLVM/Clang 17+ is the superior compiler for RVV intrinsic-heavy code** (Phases 1–3) due to its earlier and more complete coverage of the RVV 1.0 intrinsic API and better diagnostic quality.
- A **dual-compiler workflow** — GCC for baseline and KAT correctness, Clang for vectorized cryptographic kernels — is the most defensible approach.
- **Both compilers must be built from a single `riscv-gnu-toolchain` invocation** using the `--enable-llvm` configure flag. This is the only method that natively synchronizes the Clang sysroot, ABI flags, and newlib headers with GCC without manual path surgery.
- **Spike is the mandatory primary simulator** for instruction counting and `Zvbc` correctness validation. QEMU, which is also buildable through the same meta-repository, is the **only officially supported simulator for the Dejagnu compiler test suite** and serves an indispensable but distinct role.
- **Build everything from source** using the `riscv-gnu-toolchain` meta-repository. It is the only path that guarantees a coherent, version-matched set of GCC, Clang, Binutils, Newlib, QEMU, and Spike under a single `-march` flag.

---

## 2. Compiler Head-to-Head: GCC vs. LLVM/Clang

### 2.1 GCC (riscv-gnu-toolchain)

GCC is the de-facto standard for RISC-V bare-metal development and the compiler explicitly named in the HQC Reference C codebase (`riscv64-unknown-elf-gcc`). Its strengths and limitations for this thesis are:

**Strengths:**
- **Mature bare-metal target.** The `riscv64-unknown-elf` newlib target has been production-grade since GCC 10. Linking the HQC Reference C framework, the proxy kernel interface (`pk`), and the KAT harness together is well-documented and reliable.
- **Inline assembly syntax is the gold standard.** All published RISC-V cryptography papers use GCC extended `asm` syntax. The `clmul`/`clmulh` base-case sequences described in Phase 1 (Strategy A, 31-GPR register scheduling) are easiest to express and validate in GCC `asm volatile` blocks with explicit constraint strings (`"r"`, `"=r"`, `"&r"` for early-clobber).
- **`-march` extension string is authoritative.** The GCC march parser is the reference implementation for RISC-V extension naming and ordering. Any march string that compiles cleanly in GCC is guaranteed to be syntactically valid.
- **`Zvbc` support landed in GCC 14.** The ratified RISC-V Vector Cryptography Extensions specification (which includes `Zvbc`, `Zvbb`, `Zvkg`, `Zvks`, `Zvkn`) was integrated into GCC 14 as part of the `_riscv_crypto.h` intrinsic header. This makes **GCC 14 the hard minimum** for the thesis's Tier-4 benchmarks.
- **`Zbkb` intrinsics** (`__riscv_brev8_32`, `__riscv_pack`, `__riscv_packh`, `__riscv_zip_32`, `__riscv_unzip_32`) are available via `<riscv_crypto.h>` in GCC 12+, covering Phase 3's RM→RS data-path acceleration.

**Weaknesses:**
- **GCC's RVV auto-vectorizer is immature compared to Clang's.** For the XOR-combine loops and basis-conversion sweeps in Phase 1, GCC's loop vectorizer may not reliably emit `vle64.v`/`vxor.vv`/`vse64.v` with the correct LMUL grouping without explicit intrinsic annotations.
- **Diagnostic quality for RVV intrinsic misuse is poor.** A wrong element-width argument to `vluxei8.v` produces a cryptic internal error in GCC 13 rather than a clear type mismatch.

### 2.2 LLVM/Clang

LLVM's RISC-V backend has overtaken GCC in several dimensions directly relevant to this thesis.

**Strengths:**
- **RVV 1.0 intrinsic API is Clang-first.** The RISC-V Vector Intrinsic Document was developed against the LLVM toolchain. Clang 15+ provides 100% RVV 1.0 intrinsic coverage with proper type-checked overloads (`vfloat32m4_t`, `vuint64m1_t`, etc.) and better error messages when widths or LMUL arguments are inconsistent.
- **`Zvbc` vectorized butterfly intrinsics are available in LLVM 17+.** The `__riscv_vclmul_vv_u64m1` and `__riscv_vclmulh_vv_u64m1` intrinsics that directly implement Phase 1 Axis 4 are more robustly handled in Clang than GCC at the time of writing.
- **`-fsanitize=undefined` on bare-metal builds.** Clang's UBSan is compilable for the `riscv64-unknown-elf` target and is invaluable during the Step-by-step KAT assembly (Section 2.3 of the thesis plan) to catch pointer aliasing violations in ported FAFFT code.
- **`-Rpass=loop-vectorize` diagnostics.** Clang emits per-loop remarks explaining why a loop was or was not vectorized. This is directly useful when tuning the FAFFT XOR-combine phases for LMUL=4 throughput.
- **Better codegen for structured intrinsic loops.** For patterns like `vle64.v → vxor.vv → vse64.v`, Clang's instruction scheduler consistently produces tighter instruction windows than GCC 14 on the RISC-V O3 microarchitecture model.

**Weaknesses:**
- **Inline assembly constraint ergonomics.** Clang accepts GCC `asm` constraints, but edge cases in the 31-GPR scheduling sequences (especially multi-output constraints for `clmul`+`clmulh` pairs) can behave differently. All critical assembly sequences must be tested under both compilers.
- **Historically required a separate sysroot.** When built independently, Clang must be pointed at the GCC newlib sysroot: `--sysroot=/opt/riscv/riscv64-unknown-elf`. The `--enable-llvm` unified build (Section 3.2) eliminates this problem entirely: the Clang frontend is configured against the same newlib installation as GCC, resolving headers and libraries identically.

### 2.3 Verdict: Dual-Compiler Workflow

A **single-compiler reliance is a thesis risk.** The two compilers serve distinct roles that do not overlap:

| Role | Compiler | Rationale |
|---|---|---|
| KEM framework compilation (KeyGen, Encaps, Decaps, FO) | **GCC 14** | Stable bare-metal newlib ABI; matches HQC Reference C documentation |
| KAT correctness validation (all 8 assembly steps) | **GCC 14** | Authoritative reference; catches ABI regressions |
| RVV intrinsic kernels (XOR combines, `vluxei8.v`, `vclmul.vv`) | **Clang 17+** | Type-checked intrinsic API, `Rpass` diagnostics, tighter codegen |
| Inline assembly blocks (`clmul` + Barrett reduction) | **Both** | Cross-validate codegen; divergent output signals a register-constraint bug |
| Performance benchmarking (final cycle counts) | **Best of GCC/Clang** | Report both; performance delta between compilers is publishable data |

Concretely: compile the HQC framework and KAT harness with GCC, link against the RVV kernel object files compiled with Clang. This is safe because both compilers produce compatible RISC-V ELF objects under the same `lp64d` ABI. When built via `--enable-llvm`, both compilers share the same newlib installation and target triple, so mixed-compiler linking requires no additional flags.

---

## 3. Toolchain Acquisition Strategy

### 3.1 Pre-Built Binaries via `apt`

Ubuntu 22.04 LTS ships `gcc-riscv64-unknown-elf` at **GCC 12.2**. Ubuntu 24.04 LTS ships **GCC 13.2**.

**This is categorically insufficient for this thesis:**
- Neither version supports `Zvbc` (requires GCC 14).
- GCC 12's RVV intrinsic coverage is incomplete; several `vluxei8.v` overloads needed for Phase 1 Axis 2 are missing.
- The packaged `binutils` may not assemble `Zbkb` instructions (`brev8`, `pack`, `zip`) without explicit `.option arch` pragmas that interact poorly with LTO.

The `apt` route is acceptable only for quick sanity-checks of pure-C HQC reference code (Assembly Step 1 in the thesis plan). It must not be used for any optimization phase.

### 3.2 Building from `riscv-gnu-toolchain` Source (Recommended)

The canonical meta-repository at `github.com/riscv-collab/riscv-gnu-toolchain` bundles GCC, Binutils, Newlib, GDB, and Spike (as a git submodule) under a single versioned checkout. Building from source provides:

- **Exact GCC version control** — pin to GCC 14.x to guarantee `Zvbc` support.
- **Coherent `-march` propagation** — the same architecture string is passed to the GCC configure, the assembler, and Spike's configure, eliminating version skew.
- **Newlib tuned for RISC-V bare-metal** — the `riscv64-unknown-elf` target is first-class; no sysroot surgery needed.
- **Single `make install`** — compiler, assembler (`riscv64-unknown-elf-as`), linker (`riscv64-unknown-elf-ld`), object dumper (`riscv64-unknown-elf-objdump`), and the Spike ISA simulator all land under one prefix.

**Build time** on a modern 8-core machine: approximately 45–75 minutes for the newlib target. This is a one-time cost.

### 3.3 Building GCC and LLVM Separately

If the dual-compiler workflow is adopted (recommended), LLVM/Clang must be sourced independently because `riscv-gnu-toolchain` does not include it.

**LLVM build for RISC-V cross-compilation:**
```bash
cmake -G Ninja -S llvm-project/llvm -B llvm-build \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_TARGETS_TO_BUILD="RISCV" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/opt/llvm-riscv
ninja -C llvm-build install
```

LLVM requires a separate `--sysroot` pointing at the GCC newlib installation for bare-metal linking. This is a one-time configuration, not a recurring cost.

**Pre-built LLVM alternative:** The `llvm.org/releases` page provides pre-built Linux binaries for LLVM 18+, which include the RISC-V backend with `Zvbc` support. These are suitable for the RVV intrinsic development work and avoid the ~90-minute LLVM build.

### 3.4 Summary

| Method | GCC Version | Zvbc | RVV Intrinsics | Effort |
|---|---|---|---|---|
| `apt install gcc-riscv64-unknown-elf` | 12–13 | ❌ | Incomplete | Minimal |
| `riscv-gnu-toolchain` from source | 14+ (pinned) | ✅ | Complete | ~75 min build |
| Pre-built LLVM 18 binary | N/A (Clang) | ✅ | Complete (first-class) | ~5 min download |

**Recommendation:** Build `riscv-gnu-toolchain` from source for GCC 14 + Spike. Download pre-built LLVM 18 for Clang. Do not use `apt` for any optimization-phase compilation.

---

## 4. Execution & Simulation Environment

### 4.1 Spike (riscv-isa-sim) — Primary Simulator for ISA Validation

Spike is a **functionally exact ISA simulator** maintained by the RISC-V International organization. It is the canonical reference for instruction-set compliance and the correct choice for every correctness-critical task in this thesis.

**Why Spike is mandatory for this thesis's critical path:**

- **ISA string specificity.** Spike's `--isa=rv64gcv_zbc_zbb_zbkb_zvbc` flag selects exactly the extension set needed. Attempting to execute a `vclmul.vv` instruction on a Spike instance configured without `Zvbc` produces an illegal instruction trap — precisely the behavior required to validate that each thesis tier uses only its declared extensions and nothing more.
- **Instruction-commit logging.** `spike --log-commits -l ...` dumps every committed instruction to stderr. This is the thesis's **primary profiling mechanism** for counting `clmul`, `vxor.vv`, and `vluxei8.v` instruction frequencies per phase — no hardware performance counters needed. QEMU has no equivalent facility.
- **KAT vector validation at zero overhead.** The HQC Known-Answer Tests run directly on Spike via the `riscv-pk` proxy kernel without any OS or driver overhead. This is the correctness gate at each of the 8 assembly steps.
- **`Zvbc` correctness is guaranteed.** Spike's `Zvbc` support produces bit-exact results against the ratified specification. QEMU's `Zvbc` coverage tracks Spike by 6–12 months and must not be trusted for Tier-4 instruction encoding validation.

**Spike limitation:** Spike is **not cycle-accurate**. It cannot model VLEN effects, pipeline depth, load-use hazards, or cache pressure. For the final gem5 benchmarks in Month 12, Spike instruction counts serve as a cycle-count proxy only for instruction-bound kernels.

**Important caveat on the compiler test suite:** Spike's support for the Dejagnu GCC/Clang test suite is officially classified as **experimental** in the `riscv-gnu-toolchain` build system. The test suite allowlist is maintained only for QEMU. Running `make check` against Spike may produce extra failures not present in QEMU; these should not be interpreted as compiler regressions.

### 4.2 QEMU — Official Compiler Test Suite Validator

QEMU supports RISC-V in two modes: `qemu-system-riscv64` (full machine emulation) and `qemu-riscv64` (Linux user-space emulation). Its role in this project is **complementary and non-optional** — it serves a purpose that Spike explicitly does not cover.

**Why QEMU is indispensable for compiler validation:**

- **Official Dejagnu test suite support.** The `riscv-gnu-toolchain` Dejagnu port is explicitly maintained against QEMU as the reference simulator. After building the dual-compiler toolchain (GCC + Clang via `--enable-llvm`), running `make check SIM=qemu` executes the full regression suite for both compilers against an allowlist maintained to be QEMU-clean. This is the **correct method for validating that the GCC 14 and Clang builds are internally consistent** before trusting them to compile cryptographic code.
- **Faster execution for large test suites.** QEMU uses a JIT translator; it is significantly faster than Spike's interpretation loop for the thousands of short programs in the Dejagnu suite.
- **Linux `glibc` target testing.** The `riscv64-unknown-linux-gnu` target, needed for Linux-hosted integration tests (e.g., running `ctgrind` for constant-time validation of HQC operations), requires QEMU's Linux user-space mode. Spike's `riscv-pk` proxy kernel does not support `glibc`-linked binaries.
- **SHAKE-256 and RS table integration tests.** Smoke-testing the SHAKE-256 wrapper and GF(2^8) table initialization code in a Linux environment before bare-metal porting is best done on QEMU, where `printf` and `malloc` are available without newlib limitations.

**Where QEMU must not be used:**
- **Zvbc instruction encoding validation** — Spike only.
- **Instruction counting and profiling** — Spike only.
- **Final Tier benchmark reproducibility** — Spike only (extension set is precisely controlled).

**Verdict:** QEMU and Spike have strictly non-overlapping roles. Spike owns correctness and profiling for cryptographic kernels; QEMU owns compiler regression testing and Linux-hosted integration. Both are required; neither can substitute for the other.

### 4.3 gem5

gem5 is the **cycle-accurate microarchitectural simulator** used in Phase 4 (Month 12) for final performance evaluation. It models pipeline width, VLEN, cache hierarchy, and branch predictor behavior — essential for comparing the four thesis tiers on a realistic out-of-order core model.

gem5 is entirely separate from both the toolchain and Spike. It consumes standard RISC-V ELF binaries produced by the GCC/Clang toolchain. The thesis's gem5 configuration (`RISC-V O3 + vector pipeline`) must be built and validated as a distinct Phase 4 activity and is out of scope for the toolchain acquisition discussion here.

---

## 5. Simulator Acquisition Strategy

### 5.1 Unified Build via `riscv-gnu-toolchain` — Both Spike and QEMU

The `riscv-gnu-toolchain` repository supports building **both** Spike and QEMU as part of the same meta-build. Both simulators are available as submodule targets and are built against the same configure-time architecture flags as the compilers.

Simulators are selected using the `SIM` variable on the `make` command line:

```bash
# Build Spike alongside the compiler
make newlib -j$(nproc) build-sim SIM=spike

# Build QEMU alongside the compiler
make newlib -j$(nproc) build-sim SIM=qemu
```

Both can be built in the same environment by invoking the targets in sequence after a single `./configure` invocation.

Building both simulators through the meta-repository provides the same coherent architecture benefit for each:

- **Spike** is configured against the `--with-arch` string, ensuring its ISA decoder recognizes every `Zvbc` opcode emitted by the co-built GCC/Clang.
- **QEMU** is built at a revision tested with the same GCC/Clang version, providing the correct allowlist baseline for `make check`.
- **`riscv-pk`** (the proxy kernel for Spike bare-metal loading) is also a submodule, built with `make pk` and installed under the same prefix.
- **Everything lands under `--prefix`** — no `PATH` manipulation or `LD_LIBRARY_PATH` surgery after `make install`.

### 5.2 Version Skew: The Core Risk of Independent Builds

Building either simulator independently carries the same fundamental risk: **version skew**. If an independently-built Spike's `Zvbc` encoding was committed at a different date than the GCC 14 `Zvbc` encoding baked into `riscv-gnu-toolchain`'s submodule lock, the result is either a silent wrong-answer bug or a spurious illegal-instruction trap. For a cryptographic correctness thesis where bit-exact KAT matching is the primary validation mechanism, this class of silent failure is unacceptable.

The unified build eliminates this risk by construction: the Spike and QEMU submodule SHAs are locked to the same commit that was CI-tested against the bundled GCC and Clang.

### 5.3 Dejagnu Test Suite: QEMU is the Maintained Target

The `riscv-gnu-toolchain` Dejagnu port officially supports three simulator backends via `make check SIM=<value>`:

| `SIM=` value | Status | Use case |
|---|---|---|
| `qemu` | **Officially maintained** — allowlist kept current | Compiler regression testing, toolchain validation |
| `gdb` | Supported | Remote GDB stub testing |
| `spike` | **Experimental** — extra failures expected | ISA-level bare-metal debugging only |

This means that after building the unified toolchain, the correct command to validate the GCC + Clang build is:

```bash
make check SIM=qemu
```

Running `make check SIM=spike` will report extra failures unrelated to compiler correctness. These must not be treated as regressions. For the thesis, QEMU-based `make check` is the **only valid compiler regression gate**; Spike is used exclusively for cryptographic kernel instruction-level validation.

### 5.4 Recommendation

Build **both** Spike and QEMU through the unified `riscv-gnu-toolchain` meta-build. Do not build either simulator independently. gem5 is always built independently as a Phase 4 activity.

---

## 6. Final Verdict & Actionable Setup

### Recommended Toolchain Stack

| Tool | Version | Source | Role |
|---|---|---|---|
| `riscv64-unknown-elf-gcc` | GCC 14.x | `riscv-gnu-toolchain` (`--enable-llvm`) | HQC framework, KAT harness, inline assembly |
| `riscv64-unknown-elf-clang` | LLVM/Clang 17+ | `riscv-gnu-toolchain` (`--enable-llvm`) | RVV intrinsic kernels, UBSan, vectorization diagnostics |
| `riscv64-unknown-elf-ld` | Bundled Binutils | `riscv-gnu-toolchain` | Link |
| `spike` | Submodule-locked | `riscv-gnu-toolchain` (`build-sim SIM=spike`) | KAT validation, instruction counting, Zvbc correctness |
| `pk` (proxy kernel) | Submodule-locked | `riscv-gnu-toolchain` (`make pk`) | Bare-metal ELF loader for Spike |
| `qemu-riscv64` | Submodule-locked | `riscv-gnu-toolchain` (`build-sim SIM=qemu`) | Dejagnu compiler regression suite, Linux integration tests |
| `gem5` | Independent | Built separately (Phase 4) | Cycle-accurate microarchitectural benchmarks |

---

### Step 1 — Clone and Configure the Unified Toolchain

```bash
# 1. Clone the meta-repository with all submodules
#    (includes GCC, LLVM, Binutils, Newlib, Spike, QEMU, riscv-pk)
git clone https://github.com/riscv-collab/riscv-gnu-toolchain
cd riscv-gnu-toolchain
git submodule update --init --recursive

# 2. Configure for bare-metal, targeting the full HQC extension set
#    --prefix:       install root for all tools
#    --enable-llvm:  build Clang alongside GCC, with automatic sysroot wiring
#    --disable-linux: bare-metal ELF only (no Linux glibc target)
#    --with-arch:    default -march for GCC, Clang, Binutils, and Spike
#    --with-abi:     lp64d (64-bit integers, double-precision FP ABI)
./configure \
    --prefix=/opt/riscv \
    --enable-llvm \
    --disable-linux \
    --with-arch=rv64gcv_zbc_zbb_zbkb_zvbc \
    --with-abi=lp64d
```

> `--enable-llvm` is the key flag. It instructs the build system to compile Clang as part of the same invocation as GCC and to automatically set `--sysroot` in the Clang driver to the newlib installation being built alongside it. No manual sysroot configuration is required.

---

### Step 2 — Build GCC, Clang, Spike, QEMU, and `riscv-pk`

```bash
# 3. Build the newlib bare-metal target (GCC + Clang)
make newlib -j$(nproc)

# 4. Build Spike (ISA validator, instruction logger, Zvbc correctness)
make newlib -j$(nproc) build-sim SIM=spike

# 5. Build QEMU (Dejagnu test suite, Linux integration tests)
make newlib -j$(nproc) build-sim SIM=qemu

# 6. Build the proxy kernel (bare-metal ELF loader for Spike)
make pk -j$(nproc)

# 7. Install everything under /opt/riscv
make install

# 8. Add to PATH
export PATH=/opt/riscv/bin:$PATH

# 9. Verify all four tools
riscv64-unknown-elf-gcc --version
# → riscv64-unknown-elf-gcc (GCC) 14.x.x
riscv64-unknown-elf-clang --version
# → riscv64-unknown-elf-clang version 17.x.x (or 18.x.x)
spike --version
# → Spike RISC-V ISA Simulator ...
qemu-riscv64 --version
# → qemu-riscv64 version ...
```

> Steps 4, 5, and 6 can be run in any order after step 3 completes. The `make ... build-sim SIM=<value>` targets are independent of each other. All four `make` invocations share the configure-time `--with-arch` flag, so Spike, QEMU, GCC, and Clang all operate on the identical extension universe.

---

### Step 2b — Validate the Compiler Build with Dejagnu (QEMU)

```bash
# Run the official Dejagnu regression suite against QEMU
# SIM=qemu is the only maintained allowlist — use it for all compiler validation
make check SIM=qemu

# Expected: zero failures relative to the QEMU allowlist
# Do NOT use SIM=spike for make check — Spike is experimental for this purpose
# and will report extra failures unrelated to compiler correctness
```

---

### Step 3 — Validate the Setup Against KAT Vectors

```bash
# Compile HQC Reference C (Step 1 of thesis assembly plan)
riscv64-unknown-elf-gcc \
    -O2 \
    -march=rv64gcv_zbc_zbb_zbkb_zvbc \
    -mabi=lp64d \
    -o hqc128_kat hqc128_kat.c \
    -I/opt/riscv/riscv64-unknown-elf/include

# Run on Spike with explicit ISA string and instruction logging
spike \
    --isa=rv64gcv_zbc_zbb_zbkb_zvbc \
    --log-commits \
    /opt/riscv/riscv64-unknown-elf/bin/pk \
    ./hqc128_kat \
    2> kat_instruction_log.txt

# Verify KAT output (should print "PASS" for all test vectors)
# Inspect instruction log for clmul/vxor.vv frequency baseline
grep -c 'clmul' kat_instruction_log.txt
grep -c 'vxor.vv' kat_instruction_log.txt
```

> This command establishes the Phase 0 baseline instruction counts that all subsequent optimization phases are measured against. The `--isa` flag on Spike is redundant when the proxy kernel was built against the same arch string, but it is **mandatory to specify explicitly** for Tier-4 (`Zvbc`) runs where the extension set exceeds what the pk was built with.

---

### Step 4 — Mixed-Compiler Compilation Pattern

With both compilers installed under `/opt/riscv/bin` and sharing a sysroot, the dual-compiler Makefile pattern is straightforward:

```makefile
PREFIX    = /opt/riscv/bin/riscv64-unknown-elf
ARCH      = rv64gcv_zbc_zbb_zbkb_zvbc
ABI       = lp64d

# GCC: KEM framework, KAT harness, inline assembly kernels
CC_REF    = $(PREFIX)-gcc  -march=$(ARCH) -mabi=$(ABI) -O3

# Clang: RVV intrinsic kernels (vluxei8.v, vclmul.vv, vxor.vv)
# --sysroot is NOT needed — handled automatically by --enable-llvm
CC_VEC    = $(PREFIX)-clang -march=$(ARCH) -mabi=$(ABI) -O3

# Clang development flags: vectorization diagnostics + UBSan
CC_DEV    = $(CC_VEC) -Rpass=loop-vectorize -fsanitize=undefined

# Linker: use GCC driver for final link (handles newlib startup objects)
LD        = $(PREFIX)-gcc  -march=$(ARCH) -mabi=$(ABI)
```

---

### Step 5 — Per-Phase Compiler Flags Reference

| Thesis Phase | GCC flags | Clang flags |
|---|---|---|
| Phase 0 — Baseline C | `-O2 -march=rv64gc -mabi=lp64d` | same |
| Phase 1 Scalar (`Zbc`) | `-O3 -march=rv64gc_zbc_zbb -mabi=lp64d` | same |
| Phase 1 RVV (Axis 1–3) | `-O3 -march=rv64gcv_zbc_zbb_zbkb -mabi=lp64d` | + `-Rpass=loop-vectorize` |
| Phase 1 Axis 4 (`Zvbc`) | `-O3 -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d` | same |
| Phase 2 Keccak RVV | `-O3 -march=rv64gcv -mabi=lp64d` | + `-fsanitize=undefined` during dev |
| Phase 3 RM/RS (`Zbkb`) | `-O3 -march=rv64gcv_zbb_zbkb -mabi=lp64d` | same |
| Final benchmark | `-O3 -march=rv64gcv_zbc_zbb_zbkb_zvbc -mabi=lp64d -flto` | same |

> Incrementally widening `-march` at each phase serves dual purpose: it mirrors the thesis's Tier progression and makes it immediately visible when a binary accidentally uses an extension not declared for that tier (the assembler will reject the mnemonic).
