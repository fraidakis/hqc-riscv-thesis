# Reference Map: Sections → Source Files

Αυτό το αρχείο αντιστοιχίζει κάθε ενότητα/υπο-ενότητα του `sections.md` στα αρχεία
που πρέπει να διαβαστούν κατά τη συγγραφή. Χρησιμοποιείται ως context για τον AI agent.

Σύμβαση: Κάθε entry ακολουθεί τη μορφή:
`[File Path] (function/section)` - **[Category]** - Explanation

---

## Table of Contents

1. [Κεφάλαιο 1: Εισαγωγή](#κεφάλαιο-1-εισαγωγή)
2. [Κεφάλαιο 2: Θεωρητικό Υπόβαθρο Αλγορίθμων — HQC](#κεφάλαιο-2-θεωρητικό-υπόβαθρο-αλγορίθμων--hqc)
3. [Κεφάλαιο 3: Θεωρητικό Υπόβαθρο Αρχιτεκτονικής — RISC-V και Κρυπτογραφικές Επεκτάσεις](#κεφάλαιο-3-θεωρητικό-υπόβαθρο-αρχιτεκτονικής--risc-v-και-κρυπτογραφικές-επεκτάσεις)
4. [Κεφάλαιο 4: Σχετικές Εργασίες (Related Work)](#κεφάλαιο-4-σχετικές-εργασίες-related-work)
5. [Κεφάλαιο 5: Πειραματική Υποδομή και Μεθοδολογία Αξιολόγησης](#κεφάλαιο-5-πειραματική-υποδομή-και-μεθοδολογία-αξιολόγησης)
6. [Κεφάλαιο 6: Βελτιστοποίηση Πολυωνυμικού Πολλαπλασιασμού (Polynomial Multiplication)](#κεφάλαιο-6-βελτιστοποίηση-πολυωνυμικού-πολλαπλασιασμού-polynomial-multiplication)
7. [Κεφάλαιο 7: Βελτιστοποίηση Keccak, Decode και Βοηθητικών Πυρήνων](#κεφάλαιο-7-βελτιστοποίηση-keccak-decode-και-βοηθητικών-πυρήνων)
8. [Κεφάλαιο 8: Αποτελέσματα και Μετρήσεις Απόδοσης](#κεφάλαιο-8-αποτελέσματα-και-μετρήσεις-απόδοσης)
9. [Κεφάλαιο 9: Συζήτηση και Ερμηνεία Αποτελεσμάτων](#κεφάλαιο-9-συζήτηση-και-ερμηνεία-αποτελεσμάτων)
10. [Κεφάλαιο 10: Συμπεράσματα και Μελλοντικές Επεκτάσεις](#κεφάλαιο-10-συμπεράσματα-και-μελλοντικές-επεκτάσεις)
11. [Παράρτημα Α: Οδηγός Εγκατάστασης Περιβάλλοντος](#παράρτημα-α-οδηγός-εγκατάστασης-περιβάλλοντος)
12. [Παράρτημα Β: Αναπαραγωγιμότητα και Περιγραφή Artifact](#παράρτημα-β-αναπαραγωγιμότητα-και-περιγραφή-artifact)
13. [Παράρτημα Γ: Ακρωνύμια και Συντομογραφίες](#παράρτημα-γ-ακρωνύμια-και-συντομογραφίες)
14. [Supplementary: Writing Drafts](#supplementary-writing-drafts)
15. [Supplementary: Experiments Registry](#supplementary-experiments-registry)
16. [Supplementary: Key Data Artifacts](#supplementary-key-data-artifacts)

---

## Κεφάλαιο 1: Εισαγωγή

### §1.1 Η Κβαντική Απειλή και η Μετα-Κβαντική Κρυπτογραφία

* `notes/PQC-intro.txt` - **Research Notes** - Contains "Harvest now, Decrypt later" strategy, CNSA 2.0 mandate, and Shor's algorithm threat model

### §1.2 Το Πλαίσιο Τυποποίησης του NIST

* `notes/PQC-Algorithms.txt` - **Research Notes** - NIST standardization rounds overview, HQC selection timeline, "Επιλέχθηκε για standardization 11/03/2025"

### §1.3.1 Κρυπτογραφική Ευελιξία ως Απαίτηση

* `notes/PQC-intro.txt` - **Research Notes** - SIKE failure case study ("SIKE horror story"), crypto agility arguments, "Problem 1 - Crypto Agility is Destroyed"

### §1.3.2 Επιτάχυνση μέσω ISA Extensions σε Ανοιχτή Αρχιτεκτονική

* `notes/PQC-intro.txt` - **Research Notes** - RISC-V philosophy of ISA extensions replacing fixed-function accelerators
* `notes/notes_mine.md` - **Research Notes** - "Zbb, V, Zvbb part of the RVA23 standard" — mainstream adoption path evidence

### §1.4 Σκοπός και Συνεισφορά της Διατριβής

* `notes/PQC-Algorithms.txt` - **Research Notes** - HQC vs alternative PQC algorithms comparison
* `notes/notes_detailed.md` (Discovery 3) - **Research Notes** - Summary speedup table establishing the contribution claims

### §1.5 Δομή της Εργασίας

* `docs/thesis-helper/sections.md` - **Documentation** - Authoritative thesis outline with per-section content descriptions; use as the structural reference for the roadmap paragraph

---

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο Αλγορίθμων — HQC

### §2.1 Code-Based Κρυπτογραφία

#### §2.1.1 Μαθηματική Θεμελίωση

* `notes/PQC-Algorithms.txt` - **Research Notes** - "HQC Intro", security assumptions, code-based crypto foundations
* `writing/reference_explanation.md` - **Writing Draft** - Detailed vendor code walkthrough explaining HQC mathematical foundations

#### §2.1.2 Ιστορικό Πλαίσιο

* `notes/PQC-Algorithms.txt` - **Research Notes** - McEliece genealogy, quasi-cyclic MDPC evolution, HQC positioning vs BIKE/Classic McEliece

### §2.2 Ο Αλγόριθμος HQC

#### §2.2.1 Ομάδα Σχεδιασμού και Πορεία Τυποποίησης

* `notes/PQC-Algorithms.txt` - **Research Notes** - NIST round progression, sparse→dense spec change (2024), security parameters table
* `vendor/hqc-ref/CHANGELOG` - **Reference Code** - Upstream HQC changelog showing spec revisions across NIST rounds

#### §2.2.2 Λειτουργία KEM: KeyGen, Encaps, Decaps

* `writing/reference_explanation.md` - **Writing Draft** - "Phase 6: The Core Scheme", KEM flow pseudocode
* `src/hqc.c` (`hqc_pke_keygen`, `hqc_pke_encrypt`, `hqc_pke_decrypt`) - **Source Code** - Actual implementation of the three KEM operations showing polynomial operations, hash calls, and FO transform re-encryption check
* `vendor/hqc-ref/src/common/kem.c` - **Reference Code** - Vendor IND-CCA2 KEM wrapper (Encaps/Decaps + FO transform) used as the unmodified algorithmic baseline
* `vendor/hqc-ref/src/ref/hqc.c` - **Reference Code** - Vendor PKE primitives (keygen / encrypt / decrypt) underlying the KEM

#### §2.2.3 Υπολογιστική Δομή και Κατανομή Κόστους

* `notes/notes_detailed.md` (Discovery 6) - **Research Notes** - "polynomial multiplication was originally about 96% of decaps", pie chart data source
* `src/hqc.c` - **Source Code** - Shows which kernels (vect_mul, hash_*, code_decode) are called in each KEM operation
* `scripts/report/generate_pie_charts.py` - **Script** - Generates PDF pie charts showing instruction breakdown by category from trace hotspot CSVs; produces the §2.2.3 cost distribution pie chart
* `analysis/profiles/traces/scalar/ref_scalar/` - **Data Report** - Trace hotspot CSVs consumed by the pie chart generator
* `analysis/reports/charts/pie_ref-scalar.pdf` - **Figure** - Pre-rendered pie chart of reference scalar HQC instruction breakdown (polymul ~96%, Keccak, decode, misc)

### §2.3 Πολυωνυμικός Πολλαπλασιασμός σε GF(2)[x]

* `optimizations/multiplication.md` §1 - **Optimization Notes** - "Baseline Architecture and Starting Point", GF(2)[x] ring definition, Karatsuba mathematical theory
* `vendor/hqc-ref/src/ref/gf2x.c` - **Reference Code** - Vendor bit-serial GF(2)[x] multiplication used as the mathematical reference implementation

### §2.4 Συναρτήσεις Κατακερματισμού SHA-3 / Keccak

* `optimizations/keccak.md` §1 - **Optimization Notes** - "Baseline Architecture", Sponge Construction, Keccak-f[1600] round structure (θ, ρ, π, χ, ι)
* `vendor/hqc-ref/lib/fips202/fips202.c` - **Reference Code** - Vendor FIPS-202 SHA-3 / SHAKE reference (Keccak-f[1600] permutation, 5-step round, sponge absorb/squeeze)
* `vendor/hqc-ref/lib/fips202/fips202.h` - **Reference Code** - FIPS-202 public API declarations (SHAKE-256 incremental, SHA3-256/512)

### §2.5 Κώδικες Διόρθωσης Σφαλμάτων (Reed-Muller, Reed-Solomon)

* `optimizations/decode.md` §1 - **Optimization Notes** - "Baseline Architecture", RM(1,7), RS over GF(2^8), concatenated code pipeline, Berlekamp-Massey theory
* `vendor/hqc-ref/src/ref/reed_muller.c` - **Reference Code** - Vendor scalar Reed-Muller decoder (Hadamard transform + peak find) baseline
* `vendor/hqc-ref/src/ref/reed_solomon.c` - **Reference Code** - Vendor scalar Reed-Solomon decoder (syndromes / Berlekamp-Massey / Chien / Forney) baseline
* `vendor/hqc-ref/src/common/code.c` - **Reference Code** - Vendor concatenated decode glue: orchestrates RM (inner) + RS (outer) pipeline
* `vendor/hqc-ref/src/common/fft.c` - **Reference Code** - Vendor recursive Gao-Mateer additive FFT (`fft_rec`) — baseline for §7.2.4 root-finding rewrite

---

## Κεφάλαιο 3: Θεωρητικό Υπόβαθρο Αρχιτεκτονικής — RISC-V και Κρυπτογραφικές Επεκτάσεις

### §3.1 Σχεδιαστικές Αρχές και Βασική Αρχιτεκτονική RV64

#### §3.1.1 Ιστορικό Πλαίσιο και Φιλοσοφία

* `notes/notes_mine.md` - **Research Notes** - RVA23 profile mandatory extensions, mainstream adoption evidence

#### §3.1.2 Βασικά Χαρακτηριστικά RV64I

* — (textbook material, no project-specific source)

#### §3.1.3 Σύμβαση Ονοματοδοσίας Επεκτάσεων και Standard Profiles

* `notes/notes_mine.md` - **Research Notes** - RVA23 profile mandatory extensions, mainstream adoption evidence
* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Concrete ISA strings used: `rv64gc_zbc_zbb_zbs`, `rv64gcv_zbc_zbb_zba_zvbc_zvbb` — demonstrates modular extension composition

#### §3.1.4 Σύγκριση Σχεδιαστικής Φιλοσοφίας με ARM και x86

* `notes/notes_mine.md` - **Research Notes** - "Architectural Comparison: SHA-2 vs SHA-3", ARM EOR3/RAX1 vs RISC-V Zvbb comparison

### §3.2 Scalar Κρυπτογραφικές Επεκτάσεις (Zbc, Zbb, Zbkb)

* `optimizations/multiplication.md` §3.2.1 - **Optimization Notes** - "Native clmul/clmulh Injection", Zbc description and before/after analysis
* `src/common/gf.c` (`gf_mul`, `gf_square`) - **Source Code** - Example of Zbc usage via `__riscv_clmul_64` intrinsic (implementation details in §6.2)
* `src/mult/karatsuba_zbc/gf2x_clmul.c` (`clmul_leaf`) - **Source Code** - Example of clmul/clmulh pair usage (implementation details in §6.2)

### §3.3 Αρχιτεκτονική Διανυσματικής Μονάδας (RVV 1.0)

#### §3.3.1 Γενική Αρχιτεκτονική και Vector-Length Agnosticism

* `writing/VLEN_scaling.md` - **Writing Draft** - VLEN, LMUL, VLA concepts with concrete benchmark data

#### §3.3.2 Control/Status Registers και Παραμετροποίηση

* `writing/VLEN_scaling.md` "Instruction Budget Breakdown" - **Writing Draft** - VLMAX formula, SEW/LMUL interaction

#### §3.3.3 Strip-Mining και ο Μηχανισμός vsetvl

* `sim/common/mem.c` (`memcpy`) - **Source Code** - Minimal worked example of RVV strip-mining loop: vsetvl + vle8 + vse8 pattern
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`rvv_xor3_u64`) - **Source Code** - Clean VLA helper demonstrating strip-mining with SEW=64, LMUL=1

#### §3.3.4 Tail/Mask Policies και Κατηγορίες Εντολών

* — (textbook material; concrete examples in §3.3.3 and §3.3.5)

#### §3.3.5 Motivating Example — XOR Loop σε RVV

* `src/vector.c` (`vect_add`) - **Source Code** - RVV XOR loop: strip-mined vxor of two polynomial vectors — direct motivating example

### §3.4 Διανυσματικές Κρυπτογραφικές Επεκτάσεις (Zvbc, Zvbb)

* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`vclmul_leaf`) - **Source Code** - Demonstrates vclmul.vv/vclmulh.vv usage for vectorized carry-less multiply
* `src/keccak/fips202_zvbb_x2.c` (`KeccakF1600_StatePermute_x2`) - **Source Code** - Demonstrates vror.vv/vandn.vv (Zvbb) usage for vectorized Keccak rotations and χ step
* `vendor/rvv-keccak/fips202_rv64imbv_hybrid_x4.S` - **Reference Code** - Third-party RVV+Zvbb (imbv) Keccak x4 hybrid assembly reference
* `vendor/rvv-keccak/fips202_rv64imv_hybrid_x4.S` - **Reference Code** - Third-party base-RVV (imv, no Zvbb) Keccak x4 hybrid assembly — counterpart used to isolate Zvbb's contribution
* `vendor/rvv-keccak/fips202x_rv64.c` - **Reference Code** - C glue around the assembly cores; exposes batched SHA3/SHAKE APIs (x2/x3/x4) consumed by external benchmarks
* `vendor/rvv-keccak/fips202x.h` - **Reference Code** - Public API + macros for the rvv-keccak batched implementations

### §3.5 Μέθοδοι Προγραμματισμού RISC-V — Φάσμα Αφαίρεσης

* `sim/common/startup.S` - **Source Code** - Pure assembly example: bare-metal boot, CSR manipulation, vector unit enable
* `src/common/bench.c` (`hqc_rdcycle`, `hqc_rdinstret`) - **Source Code** - Inline assembly example: `__asm__ volatile("csrr %0, 0xb00")` for CSR reads
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` - **Source Code** - C intrinsics example: `__riscv_vclmul_vv_u64m1()`, `__riscv_vsetvl_e64m1()`
* `src/common/gf.c` (`gf_mul`) - **Source Code** - C intrinsics example: `__riscv_clmul_64()` scalar Zbc intrinsic
* `src/vector.c` (`vect_add`) - **Source Code** - Auto-Lowering example: plain C with Zba/Zbs auto-lowered by compiler

---

## Κεφάλαιο 4: Σχετικές Εργασίες (Related Work)

### §4.1 Υλοποιήσεις HQC σε RISC-V

* `notes/notes_detailed.md` §5.3.1–5.3.2 - **Research Notes** - Cross-architecture comparison data, prior RISC-V HQC work analysis
* `docs/literature/RISC-V/!Optimizing HQC using Frobenius Additive FFT on a RISC-V-based SoC - Paper.pdf` - **Literature** - Bjørndal et al. FAFFT RISC-V SoC paper (canonical RISC-V HQC prior work)
* `docs/literature/RISC-V/!Optimizing HQC using Frobenius Additive FFT on a RISC-V-based SoC - Presentation.pdf` - **Literature** - Companion presentation slides
* `docs/literature/RISC-V/!!!secuelec_rennes_phoenix_prez.pdf` - **Literature** - PHOENIX crypto-agile HW-sharing presentation
* `docs/literature/RISC-V/Code-based_Cryptography_in_IoT_A_HW_SW_Co-Design_of_HQC.pdf` - **Literature** - HQC HW/SW co-design IoT paper

### §4.2 Υλοποιήσεις HQC σε x86 και ARM

* `docs/literature/Other/OptHQC Optimize HQC for High-Performance.pdf` - **Literature** - OptHQC x86/AVX2 paper (state-of-the-art x86 HQC)
* `docs/literature/Other/!!!!!!!!! Faster GF(2)[x] polynomial multiplication using SIMD instructions.pdf` - **Literature** - SIMD GF(2)[x] multiplication techniques (vpclmulqdq context)
* `docs/literature/Other/!Efficient Polynomial Multiplication for HQC on ARM Cortex-M4.pdf` - **Literature** - Cortex-M4 polymul optimization (FAFFT-CRT, radix-16+Karatsuba)
* `docs/literature/Other/!BIKE optimization for the Intel Haswell and ARM Cortex-M4.pdf` - **Literature** - BIKE comparator workload on Haswell/M4
* `docs/literature/Other/Accelerating HQC with Additive FFT.pdf` - **Literature** - Additive FFT acceleration approach analysis
* `docs/literature/Other/A High Efficiency Hardware Design for the Post-Quantum KEM HQC.pdf` - **Literature** - HQC dedicated HW design contrast point
* `docs/literature/Other/Efficient Hardware Implementation of Constant Time Sampling for HQC.pdf` - **Literature** - Sampling HW reference for §10.3 future-CT discussion
* `docs/literature/Other/LEAP_Lightweight_and_Efficient_Accelerator_for_Sparse_Polynomial_Multiplication_of_HQC.pdf` - **Literature** - LEAP sparse-polymul accelerator (sparse×dense regime — superseded by NIST 2024 spec change)
* `docs/literature/PHOENIX - Crypto-Agile Hardware Sharing.pdf` - **Literature** - PHOENIX FPGA crypto-agile sharing paper
* `docs/literature/HQC Optimization Benchmarks Research.txt` - **Literature** - Aggregated benchmark numbers from published HQC implementations

### §4.3 Γενικότερες PQC Εργασίες στο RISC-V

* `notes/PQC-Algorithms.txt` - **Research Notes** - "Compatibility. Previous works focused on older HQC versions"

### §4.4 Αποτίμηση Κενού και Τοποθέτηση Παρούσας Εργασίας

* `notes/notes_detailed.md` §5.3.1–5.3.2 - **Research Notes** - Evidence that no prior end-to-end RISC-V HQC implementation exists
* `notes/PQC-Algorithms.txt` - **Research Notes** - Gap analysis: no Zbc/Zvbc exploitation in HQC literature

---

## Κεφάλαιο 5: Πειραματική Υποδομή και Μεθοδολογία Αξιολόγησης

### §5.1 Αρχιτεκτονική Εξομοιωτών (Spike ISS, QEMU)

* `docs/METHODOLOGY.md` §11, §13.1, §13.3 - **Documentation** - Spike ISS semantics, QEMU role, dual-simulator strategy
* `sim/spike/run.sh` - **Script** - Spike invocation wrapper with ISA string and memory configuration
* `sim/qemu/run.sh` - **Script** - QEMU virt machine invocation for alternative simulation backend

### §5.2 Cross-Compilation Toolchains και Ρυθμίσεις Μεταγλώττισης

* `writing/compiler-comparison.md` - **Writing Draft** - Full GCC vs Clang analysis, toolchain files, `-march` flags, LTO/PGO workflow
* `toolchain/riscv-clang.cmake` - **Build Config** - CMake toolchain file for Clang cross-compilation with RISC-V target triple
* `toolchain/riscv-gcc.cmake` - **Build Config** - CMake toolchain file for GCC cross-compilation
* `toolchain/toolchain.env` - **Build Config** - Environment variables for toolchain paths (RISCV prefix, sysroot)
* `cmake/options.cmake` - **Build Config** - ISA march strings, LTO/PGO flags, VLEN matrix configuration
* `cmake/hqc_core.cmake` - **Build Config** - Core HQC library CMake module: source file lists, compile definitions, include paths

### §5.3 Μεθοδολογία Benchmarking και Περιορισμοί Μετρικής

* `docs/METHODOLOGY.md` §11, §12 - **Documentation** - "Primary metric: retired instruction count", CPI dilemma, instret ≠ cycles analysisold
* `src/common/bench.c` (`hqc_rdcycle`, `hqc_rdinstret`) - **Source Code** - CSR read implementation: mcycle (0xb00) and minstret (0xb02) in M-mode
* `src/common/bench.h` - **Source Code** - Public bench API + Spike HTIF exit/print declarations
* `scripts/report/cpi/cpi_model.py` - **Script** - CPI estimation model from instruction mix histograms — converts instret to estimated cycles
* `scripts/report/cpi/cost_table.py` - **Script** - Per-instruction-class cost table for CPI model parameterization
* `scripts/report/cpi/cpi_report.py` - **Script** - Generates formatted CPI reports from model output
* `scripts/report/cpi/static_analyzer.py` - **Script** - Static instruction mix analyzer for ELF binaries — feeds CPI model
* `scripts/report/cpi/__init__.py` - **Script** - Package init for the CPI reporting toolkit
* `scripts/report/parse_histogram.py` - **Script** - Parses Spike PC histogram output into per-function instruction counts
* `scripts/report/map_pcs.py` - **Script** - Maps program counter addresses to function names via DWARF debug symbols
* `tests/report/cpi/conftest.py` - **Script** - pytest fixtures for the CPI reporting unit tests
* `analysis/profiles/cpi/reports/` - **Data Report** - Generated CPI reports per security level with cycle estimates
* `analysis/profiles/histograms/` - **Data Report** - PC histogram data for instruction mix analysis
* `analysis/profiles/traces/` - **Data Report** - Spike instruction-trace dumps used for hot-path identification
* `analysis/profiles/cache/` - **Data Report** - Cache simulation outputs (scalar/vector) used in CPI sensitivity discussion
* `analysis/profiles/memory/` - **Data Report** - Memory access pattern profiles

### §5.4 Επαλήθευση Ορθότητας (KAT Tests)

* `docs/ARCHITECTURE.md` - **Documentation** - KAT flow description, 300-vector validation methodology
* `scripts/test/run_kat.sh` - **Script** - Automated KAT runner: builds ELF, runs on Spike, validates against NIST response files
* `tests/kat/PQCkemKAT_128.rsp` - **Test Data** - NIST Known Answer Test vectors for HQC-128 (100 vectors)
* `tests/kat/PQCkemKAT_192.rsp` - **Test Data** - NIST KAT vectors for HQC-192
* `tests/kat/PQCkemKAT_256.rsp` - **Test Data** - NIST KAT vectors for HQC-256
* `tests/kat/kat_gen.c` - **Source Code** - KAT generation harness that exercises full KEM cycle and compares against .rsp files
* `tests/kat/kat_check.py` - **Script** - Python KAT validation script for automated CI checking
* `tests/kat/gen_kat_seeds.py` - **Script** - Generates deterministic seed headers from .rsp files for embedded KAT
* `tests/kat/generated/` - **Generated Code** - Pre-extracted KAT seeds for bare-metal execution without .rsp file parsing

### §5.5 Bare-Metal Πλαίσιο Εκτέλεσης

#### §5.5.1 Boot Sequence

* `sim/common/startup.S` - **Source Code** - Bare-metal boot: stack setup, mstatus.VS enable, BSS zeroing, HTIF exit via tohost
* `sim/common/link.ld` - **Source Code** - Linker script: memory map at 0x80000000, 128MB RAM, .text/.rodata/.bss/.stack layout, 512KB stack

#### §5.5.2 Διαχείριση Μνήμης και Runtime Υποστήριξη

* `sim/common/mem.c` (`memcpy`, `memset`, `memmove`) - **Source Code** - Custom -nostdlib memory routines with RVV fast-paths (LMUL=8 vectorized memcpy/memset)
* `sim/common/printf_stub.c` - **Source Code** - Minimal printf stub for bare-metal environment without libc
* `src/common/rng/aes_ctr_drbg.c` - **Source Code** - AES-CTR-DRBG deterministic RNG for KAT reproducibility (NIST-specified DRBG)
* `src/common/rng/aes_ctr_drbg.h` - **Source Code** - DRBG public API
* `src/common/rng/tiny_aes.c` - **Source Code** - Minimal AES-128 implementation backing the CTR-DRBG (no external dependencies)
* `src/common/rng/tiny_aes.h` - **Source Code** - tiny_aes public API
* `src/common/randombytes.c` - **Source Code** - Random byte generation interface bridging DRBG to HQC sampling functions
* `experiments/manifest.yaml` (EXP-mem-fastpath) - **Experiment Metadata** - RVV memcpy/memmove fast-path promotion record with −1.62% KEM instret (promoted into `sim/common/mem.c`)

#### §5.5.3 Καθαρότητα Μέτρησης (Measurement Purity)

* `src/common/bench.c` - **Source Code** - Measurement purity: no OS jitter, direct M-mode CSR access, HTIF console output
* `tests/bench/bench_kem.c` - **Source Code** - Benchmark harness design: warmup pass + timed iterations, CSV output format

### §5.6 Εφαρμογή Μεθόδων Προγραμματισμού στην Παρούσα Εργασία

* `sim/common/startup.S` - **Source Code** - Pure Assembly (.S): bare-metal boot sequence
* `src/common/bench.c` (`hqc_rdcycle`) - **Source Code** - Inline Assembly: CSR reads and `wfi` instruction
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` - **Source Code** - C Intrinsics: RVV + Zvbc intrinsics for vectorized carry-less multiply
* `src/keccak/fips202_zvbb_x2.c` - **Source Code** - C Intrinsics: Zvbb intrinsics for vectorized Keccak rotations
* `src/common/gf.c` - **Source Code** - C Intrinsics: scalar Zbc `__riscv_clmul_64()` for GF(2^8) arithmetic
* `src/vector.c` (`vect_add`) - **Source Code** - Auto-Lowering: plain C with RVV strip-mining; Zba/Zbs auto-lowered by compiler

### §5.7 Baseline Profiling

* `analysis/benchmarks/isolated/scalar/ref_scalar/` - **Data Report** - Reference scalar baseline instret measurements per security level
* `analysis/reports/ref_scalar_L128_report.md` - **Data Report** - Detailed profiling report for reference scalar HQC-128
* `analysis/reports/ref_vector_L128_report.md` - **Data Report** - Detailed profiling report for reference vector HQC-128 (compiler auto-vectorization baseline)
* `analysis/reports/charts/pie_ref-scalar.pdf` - **Figure** - Pie chart of reference scalar instruction breakdown (baseline denominator for all speedups)
* `analysis/reports/charts/pie_ref-vector.pdf` - **Figure** - Pie chart of reference vector instruction breakdown (compiler auto-vectorization baseline)
* `notes/notes_detailed.md` (Discovery 6) - **Research Notes** - Baseline pie chart data: polymul ~96% of total instret

---

## Κεφάλαιο 6: Βελτιστοποίηση Πολυωνυμικού Πολλαπλασιασμού (Polynomial Multiplication)

### §6.1 Σχεδιαστικό Πλαίσιο — Hardware Tiers και Implementation Paths

#### §6.1.1 Tier 1 — Scalar Deployment (rv64gc + Zbc/Zbb)

* `notes/notes_detailed.md` (Discovery 3) - **Research Notes** - "3-Tier Hardware Profile", Tier 1/2/3 table
* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Tier 1 ISA string: `rv64gc_zbc_zbb_zbs`

#### §6.1.2 Tier 2 — Base-RVV Deployment (rv64gcv χωρίς Zvbc/Zvbb)

* `cmake/options.cmake` - **Build Config** - Tier 2 ISA string: `rv64gcv_zbc_zbb_zba`

#### §6.1.3 Tier 3 — Full Vector-Crypto Deployment (rv64gcv + Zvbc/Zvbb)

* `cmake/options.cmake` - **Build Config** - Tier 3 ISA string: `rv64gcv_zbc_zbb_zba_zvbc_zvbb`
* `scripts/implemented_variants.txt` - **Build Config** - List of all implemented polymul variants (ref_scalar, ref_vector, karatsuba_zbc, karatsuba_rvv, karatsuba_zvbc)

### §6.2 Scalar Path — Karatsuba με Zbc

#### §6.2.1 Μετάβαση από Bit-Serial σε Word-Level Πολλαπλασιασμό

* `optimizations/multiplication.md` §3 (Track A) - **Optimization Notes** - Word-granularity clmul injection, bit-serial → word-level transition
* `src/mult/karatsuba_zbc/gf2x_clmul.c` (`clmul_leaf`) - **Source Code** - Scalar Karatsuba leaf using `__riscv_clmul_64`/`__riscv_clmulh_64` pairs for 64-bit polynomial products
* `vendor/hqc-ref/src/ref/gf2x.c` - **Reference Code** - Original bit-serial schoolbook implementation for comparison
* `docs/proposals/roadmap_karatsuba_v1.md` - **Documentation** - Optimization roadmap v1 (initial scalar Karatsuba plan)

#### §6.2.2 Assign-on-First-Write — Εξάλειψη memset

* `optimizations/multiplication.md` §3 - **Optimization Notes** - Memset-free discipline, assign-on-first-write pattern, −23% instret
* `src/mult/karatsuba_zbc/gf2x_clmul.c` - **Source Code** - Implementation uses direct assignment (`=`) on first write to each output word, eliminating memset
* `docs/proposals/karatsuba_zvbc_v2_memset_cycle-explanation.md` - **Documentation** - Profiling-discrepancy analysis explaining why function-level memset trace (−891k) ≠ end-to-end instret delta (−23k)

#### §6.2.3 Δομή Αναδρομής και AP-2 Generator

* `scripts/gen/gen_karatsuba_specialized.py` - **Script** - Python AP-2 generator: compile-time recursion tree generation, emits specialized C headers
* `src/mult/karatsuba_zbc/karatsuba_specialized_gen.h` - **Generated Code** - Output of AP-2 generator: fully unrolled Karatsuba recursion tree for scalar path
* `optimizations/multiplication.md` §3.1.3 - **Optimization Notes** - AP-2 Generator design, "static code generation beats compiler"
* `notes/notes_detailed.md` (Discovery 1) - **Research Notes** - "ABI obligations are structural" — why generated code outperforms compiler
* `docs/proposals/roadmap_karatsuba_v2.md` - **Documentation** - Roadmap v2 documenting the AP-2 generator promotion plan

#### §6.2.4 Unrolling και Βάθος Αναδρομής

* `scripts/gen/gen_karatsuba_specialized.py` (`emit_function`) - **Script** - Controls unroll depth, two-way pipeline, hi⊕lo fusion generation
* `src/mult/karatsuba_zbc/karatsuba_specialized_gen.h` - **Generated Code** - Fully unrolled recursion tree with zero runtime dispatch
* `experiments/promoted/EXP-karatsuba-zbc-leaf-fixed/gf2x_clmul.c` - **Experiment** - Promoted fixed-size leaf (`clmul_leaf_n8`, T=16, −3.21% polymul)

#### §6.2.5 Worked Example — Πλήρης Αναδρομή HQC-128

* `src/mult/karatsuba_zbc/gf2x_clmul.c` (`vect_mul`) - **Source Code** - Entry point showing full recursion invocation for N-word polynomial multiply
* `src/common/hqc_parameters.h` - **Source Code** - HQC-128 parameters (N, VEC_N_SIZE_64) that determine recursion tree dimensions

#### §6.2.6 Απορριφθείσες Εναλλακτικές (Toom-Cook, Pointer-Bump, FAFFT)

* `writing/mult_alternatives.md` - **Writing Draft** - TC3/TC4/FAFFT/sparse analysis, measured regressions, "why Karatsuba wins"
* `experiments/EXP-toomcook3-gf2x-zbc/gf2x_clmul.c` - **Experiment** - TC3 implementation attempt with GF(4) evaluation points
* `experiments/EXP-toomcook3-gf2x-zbc/tc3_eval.h` - **Experiment** - TC3 evaluation-point matrix (GF(4) {0,1,ω,ω²,∞})
* `experiments/EXP-toomcook3-gf2x-zbc/tc3_interp.h` - **Experiment** - TC3 interpolation tables
* `experiments/EXP-toomcook3-gf2x-zbc/notes.md` - **Experiment Metadata** - TC3 abandonment notes (evaluation/interpolation overhead dominates at HQC sizes)
* `experiments/dropped/EXP-fafft-zvbc-towerfield/fafft_vclmul_towerfield.c` - **Experiment** - FAFFT tower-field source — abandoned, incompatible with vclmul.vv
* `experiments/dropped/EXP-fafft-rvv-blueprint/` - **Experiment** - FAFFT RVV blueprint — abandoned, 4-5× regression measured
* `docs/proposals/roadmap_fafft.md` - **Documentation** - FAFFT exploration roadmap (rejected after measurements)
* `notes/notes_detailed.md` §5.3.2.Β - **Research Notes** - "FAFFT Paradox", "4–5× regression" evidence

### §6.3 Base-RVV Path — Διανυσματοποίηση Recomposition

#### §6.3.1 Διανυσματοποίηση Combine και Data-Movement Βρόχων

* `optimizations/multiplication.md` §4 (Track B) - **Optimization Notes** - RVV combine loops, "leaf is the binding constraint"
* `src/mult/karatsuba_rvv/gf2x_clmul.c` (`rvv_copy_u64`, `rvv_xor_u64`, `rvv_xor3_u64`) - **Source Code** - VLA helper functions replacing scalar 4×-unrolled loops with strip-mined vector operations
* `src/mult/karatsuba_rvv/karatsuba_rvv_specialized_gen.h` - **Generated Code** - AP-2 generated tree with RVV recomposition calls instead of scalar loops
* `scripts/gen/gen_karatsuba_specialized.py` (`_rvv_recomposition`) - **Script** - Generator mode that emits RVV helper calls for recomposition stages

#### §6.3.2 Μείωση Register Pressure μέσω RVV

* `src/mult/karatsuba_rvv/gf2x_clmul.c` - **Source Code** - Vector loads/stores offload data movement from scalar register file to vector register file

### §6.4 Vector-Crypto Path — Zvbc Leaf Kernels

#### §6.4.1 Εξειδίκευση Leaf Kernels με vclmul.vv/vclmulh.vv

* `optimizations/multiplication.md` §5 (Track C) - **Optimization Notes** - vclmul_leaf design, splat-and-sweep pattern, assign-first-write
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`vclmul_leaf`) - **Source Code** - Vectorized leaf kernel: loads 16 elements into vector register, splats each b[j], computes vclmul/vclmulh pairs, accumulates results
* `src/mult/karatsuba_zvbc/karatsuba_zvbc_specialized_gen.h` - **Generated Code** - AP-2 tree with Zvbc leaf calls and RVV recomposition
* `docs/proposals/roadmap_karatsuba_zvbc_v1.md` - **Documentation** - Zvbc Karatsuba roadmap v1
* `docs/proposals/roadmap_karatsuba_zvbc_v2.md` - **Documentation** - Zvbc Karatsuba roadmap v2 (memset/cycle reconciliation)

#### §6.4.2 Schoolbook Leaf Trade-offs

* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`vclmul_leaf`) - **Source Code** - O(n²) schoolbook structure with fixed n=16 — demonstrates why leaf doesn't scale with VLEN

### §6.5 Αποτίμηση — Ανακατανομή Προφίλ μετά τη Βελτιστοποίηση Polymul

* `analysis/benchmarks/isolated/scalar/karatsuba_zbc/` - **Data Report** - Tier 1 polymul instret measurements
* `analysis/benchmarks/isolated/vector/karatsuba_rvv/` - **Data Report** - Tier 2 polymul instret measurements
* `analysis/benchmarks/isolated/vector/karatsuba_zvbc/` - **Data Report** - Tier 3 polymul instret measurements
* `analysis/reports/charts/pie_karatsuba-zbc.pdf` - **Figure** - Pie chart showing Tier 1 profile redistribution after polymul optimization
* `analysis/reports/charts/pie_karatsuba-rvv.pdf` - **Figure** - Pie chart showing Tier 2 profile redistribution after polymul optimization
* `analysis/reports/charts/pie_karatsuba-zvbc.pdf` - **Figure** - Pie chart showing Tier 3 profile redistribution after polymul optimization
* `analysis/reports/speedups.md` - **Data Report** - Computed speedup multipliers across all tiers and security levels
* `notes/notes_detailed.md` (Discovery 6) - **Research Notes** - Amdahl's law analysis: polymul reduction reveals Keccak/decode as new bottlenecks

---

## Κεφάλαιο 7: Βελτιστοποίηση Keccak, Decode και Βοηθητικών Πυρήνων

### §7.1 Επανασχεδιασμός Keccak / SHA-3

#### §7.1.1 Scalar Path (Zbb)

* `optimizations/keccak.md` §3 - **Optimization Notes** - "Track A/B — Shared Baseline: Reference Keccak with Compiler Zbb", rori auto-lowering
* `vendor/hqc-ref/lib/fips202/fips202.c` - **Reference Code** - Vendor scalar Keccak implementation serving as baseline
* `notes/notes_detailed.md` (Discovery 5) - **Research Notes** - "Zbb alone yields 37% Keccak reduction via native rori"
* `experiments/manifest.yaml` (EXP-keccak-scalar-zbb) - **Experiment Metadata** - Zbb ablation data: ≤0.30% total KEM impact, explicit builtins match auto-lowering
* `docs/proposals/RESULTS/zbb_ablation_study.md` - **Data Report** - Zbb ablation study results: rori emission count, instret delta per security level
* `docs/proposals/roadmap_keccak.md` - **Documentation** - Keccak optimization roadmap (Tracks A/B/C, RVV Paradox tracking)

#### §7.1.2 Base-RVV Path και το "RVV Paradox"

* `optimizations/keccak.md` §7.1 - **Optimization Notes** - "RVV Paradox" analysis: without vror.vv, vectorization increases instret
* `notes/notes_detailed.md` (Discovery 2) - **Research Notes** - "scalar fallback trap", "ISA completeness > ISA width"
* `experiments/dropped/EXP-keccak-rvv-only-legacy/` - **Experiment** - Base-RVV Keccak attempt — archived, 7100 instret vs 4680 ref (slower)
* `experiments/dropped/EXP-keccak-rvv-only-legacy/README.md` - **Experiment Metadata** - Legacy rvv_only Keccak archival rationale
* `experiments/dropped/EXP-keccak-rvv-x2-legacy/` - **Experiment** - RVV x2 batching attempt — archived, identical to rvv_only (7100 instret)
* `experiments/dropped/EXP-keccak-rvv-x2-legacy/README.md` - **Experiment Metadata** - rvv_x2 archival rationale

#### §7.1.3 Vector-Crypto Path (Zvbb 2-way SoA)

* `optimizations/keccak.md` §4.3–4.7, §5 - **Optimization Notes** - SoA redesign (C3), 2-way batching (C8), LMUL=8 register-wall analysis
* `src/keccak/fips202_zvbb_x2.c` (`KeccakF1600_StatePermute_x2`) - **Source Code** - 2-way SoA Keccak permutation: processes 2 states simultaneously using vror.vi/vror.vx (Zvbb), vandn.vv, vl=2 per lane
* `src/keccak/fips202_zvbb_x2.c` (`shake256_x2_absorb`, `shake256_x2_squeezeblocks`) - **Source Code** - Paired-stream SHAKE-256 wrappers enabling 2-way batching in keygen
* `src/hqc.c` (`hqc_xof_init_pair`) - **Source Code** - Integration point: keygen uses x2 paired absorb for dk/ek XOF streams simultaneously
* `notes/notes_mine.md` - **Research Notes** - "4-Way Bottleneck" analysis explaining why 2-way is the sweet spot
* `experiments/dropped/archive_keccak_c4_register_wall/README.md` - **Experiment Metadata** - 4-way attempt — abandoned due to register spilling (25×4 > 32 vector registers)
* `experiments/dropped/archive_keccak_x4_batching/fips202_rvv_x4.c` - **Experiment** - x4 batching source — abandoned, register pressure too high
* `experiments/dropped/EXP-keccak-zvbb-gather-legacy/README.md` - **Experiment Metadata** - Single-state Zvbb gather backend — superseded by x2 SoA (2838 vs 2114 instret)
* `vendor/rvv-keccak/` - **Reference Code** - Third-party RVV+Zvbb Keccak reference implementations (x2/x3/x4 hybrid assembly + C glue) for comparison

#### §7.1.4 Βελτιστοποιήσεις Absorb/Squeeze I/O

* `src/keccak/fips202_zvbb_x2.c` (`keccak_xor_bytes`, `keccak_extract_bytes`) - **Source Code** - RVV byte-streaming I/O: vle8/vxor/vse8 pipeline replacing scalar word-aligned loops
* `optimizations/keccak.md` §4.6 - **Optimization Notes** - "RVV I/O Fast Paths", keccak_xor_bytes, load64_le optimization
* `experiments/promoted/EXP-keccak-xor-bytes-rvv/src/fips202_zvbb_x2.c` - **Experiment** - Promoted source: RVV byte-streaming for absorb/squeeze, −3.48% total KEM instret
* `experiments/manifest.yaml` (EXP-keccak-xor-bytes-rvv) - **Experiment Metadata** - Promotion record with before/after instret data per security level
* `docs/proposals/RESULTS/load64_intrinsic_results.md` - **Data Report** - load64 intrinsic optimization results (absorb path)
* `docs/proposals/RESULTS/keccak_unrolling_results.md` - **Data Report** - Keccak round-loop unrolling experiment results

### §7.2 Βελτιστοποίηση Decode (Reed-Muller + Reed-Solomon)

#### §7.2.1 Scalar Path

* `optimizations/decode.md` §3 - **Optimization Notes** - Track A scalar RM+RS path, Zbc gf_mul injection
* `vendor/hqc-ref/src/ref/reed_muller.c` - **Reference Code** - Vendor scalar Reed-Muller baseline
* `vendor/hqc-ref/src/ref/reed_solomon.c` - **Reference Code** - Vendor scalar Reed-Solomon baseline
* `vendor/hqc-ref/src/common/code.c` - **Reference Code** - Concatenated decode glue baseline
* `docs/proposals/roadmap_decode.md` - **Documentation** - Decode optimization roadmap (Axes A/B/C/D)

#### §7.2.2 Base-RVV Path — Διανυσματοποίηση RM και RS Syndromes

* `optimizations/decode.md` §4 - **Optimization Notes** - Phase 3a RM vectorization, Axis A syndrome vectorization via RVV gather
* `src/decode/reed_muller_rvv.c` (`hadamard_rvv`, `find_peaks_rvv`, `expand_and_sum`) - **Source Code** - RVV-vectorized 128-pt FHT using strided loads (vlse16), butterfly add/sub, LMUL=4 for full 64-pair coverage
* `src/decode/reed_solomon_rvv.c` (`compute_syndromes_rvv`) - **Source Code** - RVV log/exp gather pipeline: vzext u8→u16, vluxei16 for GF table lookups, vredxor accumulation

#### §7.2.3 Vector-Crypto Path — Zvbc RS (Berlekamp-Massey, Forney)

* `optimizations/decode.md` §5 - **Optimization Notes** - RS Axis B (BM), Axis D (Forney), Zvbc lane-packing
* `src/decode/reed_solomon_zvbc.c` (`compute_elp_zvbc`) - **Source Code** - Zvbc-vectorized Berlekamp-Massey: vclmul-based GF(2^8) multiply pipeline for error locator polynomial
* `src/decode/reed_solomon_zvbc.c` (`compute_error_values_zvbc`) - **Source Code** - Zvbc-vectorized Forney algorithm: parallel GF(2^8) evaluation via vclmul.vv + Barrett reduction
* `src/decode/reed_solomon_zvbc.c` (`gf_reduce_vec_u64m4`) - **Source Code** - Vectorized GF(2^8) Barrett reduction for Zvbc RS pipeline
* `experiments/promoted/EXP-decode-rs-zvbc/reed_solomon_zvbc.c` - **Experiment** - Promoted Zvbc RS source (−6.9% / −3.5% / −7.3% decaps)
* `experiments/promoted/EXP-decode-rs-zvbc/README.md` - **Experiment Metadata** - Zvbc RS promotion documentation
* `docs/proposals/RESULTS/reed_solomon_zvbc_results.md` - **Data Report** - RS Zvbc optimization results with per-level instret deltas

#### §7.2.4 Vector-Crypto Path — Zvbc FFT Root-Finding (Axis C)

* `optimizations/decode.md` §5 "RS Axis C" - **Optimization Notes** - FFT root finding design, recursive-to-iterative transformation
* `src/decode/fft_zvbc.c` (`fft_iterative`) - **Source Code** - Iterative BFS FFT: level-by-level traversal replacing recursive fft_rec, eliminates O(2^M) function call overhead
* `src/decode/fft_zvbc.c` (`beta_scale_zvbc`) - **Source Code** - Vectorized GF(2^8) scaling via vclmul.vv + Barrett reduction for beta multiplication
* `src/decode/fft_zvbc.c` (`xor_broadcast_zvbc`) - **Source Code** - Contiguous vle16/vxor.vx/vse16 pipeline for XOR-broadcast operations
* `src/decode/fft_zvbc.c` (`gammas_sums_combine_zvbc`) - **Source Code** - Butterfly combination: vclmul.vv + reduce + vxor.vv for FFT recombination stage
* `src/decode/fft_zvbc.c` (`precompute_tables`, `compute_fft_betas`, `compute_subset_sums`) - **Source Code** - Precomputation of FFT tables (betas, subset sums) for iterative traversal
* `vendor/hqc-ref/src/common/fft.c` (`fft_rec`) - **Reference Code** - Recursive Gao-Mateer baseline that motivated the iterative rewrite
* `experiments/dropped/EXP-decode-rs-fft-zvbc/` - **Experiment** - Earlier FFT Zvbc iteration (dropped in favor of refined version)

#### §7.2.5 Βελτιστοποίηση GF(2^8) Reduction — Single-Pass Barrett via vclmul

* `src/decode/fft_zvbc.c` (`gf_reduce_vec_u64m4`) - **Source Code** - Single-pass Barrett reduction: 6 instructions (vsrl→vclmul→vsrl→vclmul→vxor→vand) replacing 12-instruction two-pass approach
* `src/decode/reed_solomon_zvbc.c` (`gf_reduce_vec_u64m4`) - **Source Code** - Same Barrett optimization applied in RS decode pipeline

### §7.3 Βοηθητικοί Πυρήνες

#### §7.3.1 vect_add και Bitwise Vector Πράξεις

* `optimizations/miscellaneous.md` §2.2 - **Optimization Notes** - RVV vect_add, LMUL=8, 20× reduction
* `src/vector.c` (`vect_add`) - **Source Code** - RVV-vectorized XOR of two polynomial vectors using strip-mined vxor loop

#### §7.3.2 O(weight) Direct-Scatter (vect_write_support_to_vector)

* `optimizations/miscellaneous.md` §2.1 - **Optimization Notes** - O(weight) direct-scatter replacing O(N×weight) mask-and-accumulate, 277× reduction
* `src/vector.c` (`vect_write_support_to_vector`) - **Source Code** - Direct bit-scatter: iterates over support positions and sets individual bits via `v[pos/64] |= (1ULL << (pos%64))`
* `experiments/promoted/EXP-target2-bitmanip-scatter/vector.c` - **Experiment** - Promoted scatter implementation
* `experiments/promoted/EXP-target2-bitmanip-scatter/gf2x_clmul.c` - **Experiment** - Companion polymul edits validated alongside the scatter promotion

#### §7.3.3 Πολιτικές Ευθυγράμμισης Μνήμης (Memory Alignment)

* `optimizations/miscellaneous.md` §2.3, §3 - **Optimization Notes** - support_contains_value optimization, gf_reduce inlining
* `sim/common/link.ld` - **Source Code** - Memory alignment policies: ALIGN(8) for .data/.bss, ALIGN(16) for stack

### §7.4 Βελτιστοποιήσεις μέσω Compiler Auto-Lowering (Zba, Zbs)

#### §7.4.1 Zba Address-Generation (sh1add/sh2add/sh3add)

* `optimizations/zba_ablation.md` - **Optimization Notes** - Zba ablation study: −5.6% polymul on karatsuba_zvbc, −1.0% on karatsuba_rvv, 0% on scalar
* `cmake/options.cmake` - **Build Config** - Zba promoted to vector variant march strings after ablation validation

#### §7.4.2 Zbs Single-Bit Operations (bset/bclr/bext/binv)

* `src/vector.c` (`vect_write_support_to_vector`) - **Source Code** - Primary beneficiary of Zbs: `v[idx] |= (1ULL << pos)` auto-lowered to `bset` instruction
* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Zbs enabled only in scalar march string (rv64gc_zbc_zbb_zbs)

### §7.5 Αποτίμηση — Τελικό Προφίλ μετά Βελτιστοποίηση Όλων των Πυρήνων

* `analysis/benchmarks/isolated/keccak/` - **Data Report** - Keccak isolated benchmark CSVs (ref vs optimized)
* `analysis/benchmarks/isolated/decode/` - **Data Report** - Decode isolated benchmark CSVs (ref vs rvv vs zvbc)
* `analysis/reports/karatsuba_zvbc_L128_report.md` - **Data Report** - Full Tier 3 profiling report showing balanced final profile
* `analysis/reports/charts/pie_karatsuba-zvbc.pdf` - **Figure** - Final Tier 3 pie chart showing balanced profile after all kernel optimizations (contrast with §2.2.3 baseline)

---

## Κεφάλαιο 8: Αποτελέσματα και Μετρήσεις Απόδοσης

### §8.1 Απομονωμένες Μικρο-αξιολογήσεις (Isolated Micro-benchmarks)

#### §8.1.1 bench_mul — Πολυωνυμικός Πολλαπλασιασμός

* `tests/bench/bench_mul.c` - **Source Code** - Polynomial multiply micro-benchmark: single vect_mul() call with dense/sparse inputs
* `analysis/benchmarks/isolated/scalar/` - **Data Report** - Scalar polymul instret CSVs (ref_scalar, karatsuba_zbc)
* `analysis/benchmarks/isolated/vector/` - **Data Report** - Vector polymul instret CSVs (ref_vector, karatsuba_rvv, karatsuba_zvbc)

#### §8.1.2 bench_keccak — Πλήρες Decaps Keccak Call Pattern

* `tests/bench/bench_keccak.c` - **Source Code** - Full decaps Keccak call pattern benchmark: ~6 XOF/hash initializations, ~30-50 permutations
* `analysis/benchmarks/isolated/keccak/` - **Data Report** - Reference and optimized (Zvbb x2 SoA) Keccak instret CSVs per security level

#### §8.1.3 bench_decode — Συνδυασμένη Αποκωδικοποίηση RM+RS

* `tests/bench/bench_decode.c` - **Source Code** - Combined RM+RS decode benchmark with correct buffer sizes
* `analysis/benchmarks/isolated/decode/` - **Data Report** - Reference / RVV / Zvbc decode instret CSVs

### §8.2 Αξιολόγηση Πλήρους KEM (End-to-End)

* `tests/bench/bench_kem.c` - **Source Code** - Full KEM benchmark harness: keypair/encaps/decaps with CSV output
* `analysis/benchmarks/suites/all_suite_2026-05-13_13-33/` - **Data Report** - Full KEM suite (keypair/encaps/decaps/vect_mul) across variants × security levels
* `analysis/reports/speedups.md` - **Data Report** - Computed speedup multipliers (ref→optimized) per tier/level/operation
* `scripts/report/generate_speedups.py` - **Script** - Generates thesis speedup summary table from isolated benchmark CSVs

### §8.3 Κλιμάκωση ανά Security Level

* `analysis/benchmarks/suites/all_suite_2026-05-13_13-33/` - **Data Report** - Full suite data for HQC-128/192/256 scaling analysis
* `analysis/reports/karatsuba_zvbc_L128_report.md` - **Data Report** - HQC-128 detailed breakdown
* `analysis/reports/karatsuba_rvv_L128_report.md` - **Data Report** - HQC-128 Tier 2 breakdown
* `analysis/reports/karatsuba_zbc_L128_report.md` - **Data Report** - HQC-128 Tier 1 breakdown

### §8.4 Ανάλυση Κλιμάκωσης VLEN (VLEN Scaling)

#### §8.4.1 Μοντέλο Κλιμάκωσης και Θεωρητικό Όριο

* `writing/VLEN_scaling.md` "Instruction Budget Breakdown" - **Writing Draft** - 31% vclmul_leaf, 23% Keccak, 27% recomposition — what scales vs what doesn't

#### §8.4.2 Πειραματικά Αποτελέσματα VLEN Sweep

* `analysis/benchmarks/vlen_sweep/vector/karatsuba_zvbc/` - **Data Report** - VLEN sweep instret data for Zvbc variant (256/512/1024/2048)
* `analysis/benchmarks/vlen_sweep/vector/karatsuba_rvv/` - **Data Report** - VLEN sweep instret data for RVV variant
* `writing/VLEN_scaling.md` "Benchmark Results" - **Writing Draft** - Tables per security level, VLEN=512 sweet spot identification

#### §8.4.3 Αρχιτεκτονικοί Περιορισμοί

* `writing/VLEN_scaling.md` "What Does NOT Scale" - **Writing Draft** - "Bigger VLEN ≠ fewer instructions", fixed-width coding patterns
* `writing/VLEN_scaling.md` "Failed approach" - **Writing Draft** - "threshold = VLMAX → instret INCREASED by 4–11%"
* `notes/notes_detailed.md` (Discovery 7) - **Research Notes** - Architectural limits of VLEN scaling

### §8.5 Συγκριτική Αξιολόγηση Μεταγλωττιστών (Clang vs GCC)

* `writing/compiler-comparison.md` - **Writing Draft** - Full GCC vs Clang codegen analysis, dual-compiler workflow, GCC Zvbb incompatibility
* `analysis/benchmarks/compiler_compare/scalar/` - **Data Report** - Compiler comparison CSVs for scalar variants
* `analysis/benchmarks/compiler_compare/vector/` - **Data Report** - Compiler comparison CSVs for vector variants
* `analysis/reports/compiler_analysis.md` - **Data Report** - Generated compiler showdown report with winner/margin per variant
* `scripts/report/generate_compiler_compare.py` - **Script** - Generates compiler comparison Markdown from CSV data

---

## Κεφάλαιο 9: Συζήτηση και Ερμηνεία Αποτελεσμάτων

### §9.1 Σύγκριση με Ετερογενείς Αρχιτεκτονικές

* `notes/notes_detailed.md` §5.3.1–5.3.2 - **Research Notes** - Cross-architecture table (AVX-512, Zen4, M4, FAFFT, ours), instret vs cycles methodology
* `docs/METHODOLOGY.md` §11, §12 - **Documentation** - "We do NOT claim wall-clock time on real hardware", Spike limitations
* `writing/mult_alternatives.md` - **Writing Draft** - FAFFT O(n log n) vs AP-2 Karatsuba comparison, tower-field incompatibility with vclmul.vv
* `docs/literature/HQC Optimization Benchmarks Research.txt` - **Literature** - Aggregated cross-platform benchmark numbers
* `analysis/profiles/cpi/reports/` - **Data Report** - CPI-derived cycle estimates supporting cross-architecture cycle-equivalent comparisons

### §9.2 ISA Ablation Study

#### §9.2.1 Ποσοτική Συνεισφορά Κάθε Extension

* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Ablation ISA variable: set to rv64gc_zbc to strip Zbb and measure impact
* `notes/notes_detailed.md` (Discovery 5) - **Research Notes** - "Zbb alone yields 37% Keccak reduction via native rori"
* `notes/notes_detailed.md` (Discovery 4) - **Research Notes** - "ref_vector vs ref_full_crypto — negligible difference", compiler ceiling
* `docs/proposals/RESULTS/zbb_ablation_study.md` - **Data Report** - Zbb ablation quantitative results per security level

#### §9.2.2 Zba Address-Generation — Αποτελέσματα Ablation

* `optimizations/zba_ablation.md` - **Optimization Notes** - Full Zba ablation data: −5.6% zvbc polymul, −3.3% zvbc KEM, −1.0% rvv, 0% scalar

#### §9.2.3 Zbs Single-Bit — Αποτελέσματα Ablation

* `src/vector.c` (`vect_write_support_to_vector`) - **Source Code** - Primary Zbs beneficiary: bit-scatter loop where `bset` replaces li+sll+or
* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Zbs in scalar march only (rv64gc_zbc_zbb_zbs), not in vector variants

#### §9.2.4 Compiler Auto-Lowering — Όρια και Ευρήματα

* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` - **Source Code** - Evidence that compiler does NOT auto-emit vclmul.vv from generic C — requires explicit intrinsics
* `src/keccak/fips202_zvbb_x2.c` - **Source Code** - Evidence that compiler does NOT auto-emit vror.vv from generic C — requires explicit intrinsics
* `scripts/ci/check_opcodes.sh` - **Script** - Backing opcode-verification script invoked by the workflow

### §9.3 Code Size vs Performance Trade-off

* `scripts/gen/gen_karatsuba_specialized.py` - **Script** - Generator controls unroll depth; different depths produce different .text sizes
* `analysis/reports/karatsuba_zbc_L128_report.md` - **Data Report** - Contains .text size data for scalar fully-unrolled path
* `analysis/reports/karatsuba_zvbc_L128_report.md` - **Data Report** - Contains .text size data for vector path (compact VLA helpers)
* `scripts/experiments/run_hybrid_depth_sweep.sh` - **Script** - Hybrid recursion depth sweep used to scan the depth × instret × .text trade-off

### §9.4 Σχεδιαστικές Αρχές που Γενικεύονται

* `optimizations/multiplication.md` - **Optimization Notes** - General principles: carry-less multiply efficiency, VLA strip-mining value
* `optimizations/keccak.md` - **Optimization Notes** - ISA completeness > ISA width principle
* `scripts/gen/gen_karatsuba_specialized.py` - **Script** - Metaprogramming vs hand-tuning: code generation approach

---

## Κεφάλαιο 10: Συμπεράσματα και Μελλοντικές Επεκτάσεις

### §10.1 Σύνοψη Συνεισφοράς

* `notes/notes_detailed.md` (Discovery 3) - **Research Notes** - Summary speedup table (×91, ×118, ×255 vect_mul; ×28, ×41, ×63 decaps)
* `analysis/reports/speedups.md` - **Data Report** - Final speedup numbers for thesis conclusion
* `docs/proposals/final_optim.md` - **Documentation** - Senior-author analysis of remaining ratified extensions and their (rejected/promoted) verdicts

### §10.2 Περιορισμοί

* `docs/METHODOLOGY.md` §11, §12 - **Documentation** - "We do NOT claim wall-clock time on real hardware", Spike limitations, CPI dilemma
* `docs/hardware/options_comparison.md` - **Documentation** - Available RISC-V hardware options and Zvbc availability status
* `docs/hardware/banana-pi.md` - **Documentation** - BananaPi BPI-F3 (SpacemiT K1) as potential native hardware target

### §10.3 Μελλοντική Έρευνα

* `writing/zkt_ct.md` - **Writing Draft** - Zkt constant-time formal model, engineering diagnostics — deferred to future work
* `tests/ct/ct_harness.c` - **Source Code** - Constant-time verification harness using Valgrind/ctgrind — demonstrates CT testing methodology
* `analysis/ct_verify/ctgrind/` - **Data Report** - Ctgrind XML results for various variants (karatsuba_ref, HQC-128/192/256)
* `analysis/ct_verify/dudect/` - **Data Report** - Dudect timing analysis results and summary CSVs
* `analysis/ct_verify/dudect/riscv_binaries/` - **Data Report** - Cross-compiled RISC-V binaries used for cross-platform dudect runs
* `scripts/ci/check_ct.sh` - **Script** - CI script for automated constant-time verification via ctgrind/dudect
* `notes/notes_mine.md` - **Research Notes** - "Future of RISC-V & SHA-3", PQC Task Group Keccak instructions
* `sim/rtl-marian/README.md` - **Documentation** - RTL soft-core (Marian) integration notes for future FPGA work

---

## Παράρτημα Α: Οδηγός Εγκατάστασης Περιβάλλοντος

### Α.1 Μεταγλώττιση του RISC-V Toolchain

* `toolchain/build-gcc-sysroot.sh` - **Script** - GCC cross-compiler build script with sysroot configuration
* `toolchain/build-llvm.sh` - **Script** - LLVM/Clang cross-compiler build script for RISC-V target
* `writing/compiler-comparison.md` §3 - **Writing Draft** - Build steps documentation, unified riscv-gnu-toolchain
* `.github/actions/setup-riscv-env/action.yml` - **Build Config** - Reusable GitHub Action that installs the RISC-V toolchain + Spike — canonical environment recipe

### Α.2 Εγκατάσταση του Spike ISS

* `toolchain/build-spike.sh` - **Script** - Spike ISS build script with ISA extension flags
* `toolchain/build-qemu.sh` - **Script** - QEMU build script for alternative simulation backend
* `docs/METHODOLOGY.md` §13.1 - **Documentation** - Spike build configuration and --isa flags

---

## Παράρτημα Β: Αναπαραγωγιμότητα και Περιγραφή Artifact

### Β.1 Δομή του Repository

* `README.md` - **Documentation** - Top-level project README with quick-start orientation
* `docs/ARCHITECTURE.md` - **Documentation** - Repository structure, module interactions, build system overview
* `docs/RUNBOOK.md` - **Documentation** - Script-to-table mapping, operational procedures
* `docs/PLAN.md` - **Documentation** - Project plan / chapter roadmap
* `scripts/report/repo_structure.sh` - **Script** - Generates repository structure documentation
* `.github/instructions/hqc-workflow.instructions.md` - **Documentation** - Co-design workflow protocol (experimental isolation, promotion, naming, conventional commits)

### Β.2 Οδηγίες Πλήρους Αναπαραγωγής Πειραμάτων

* `scripts/bench/run_benchmarks.sh` - **Script** - Unified benchmark runner: mul/kem/keccak/decode/suite modes with Spike
* `scripts/bench/run_profile.sh` - **Script** - Profiling entry point: cache/memory/trace/histogram modes
* `scripts/bench/lib_bench.sh` - **Script** - Shared benchmark library: variant auto-mapping, track detection, ISA string resolution
* `scripts/report/run_full_analysis.sh` - **Script** - Full analysis pipeline: runs all benchmarks and generates reports
* `CMakeLists.txt` - **Build Config** - Top-level CMake configuration for all variants and security levels
* `cmake/hqc_core.cmake` - **Build Config** - Core HQC library CMake module: source file lists, compile definitions, include paths
* `cmake/options.cmake` - **Build Config** - User-tunable build options (variant selection, security level, march, LTO/PGO)
* `tests/CMakeLists.txt` - **Build Config** - Test target definitions (KAT, bench, CT harness)
* `tests/unit/CMakeLists.txt` - **Build Config** - Unit-test target definitions
* `.github/actions/parse-variants/action.yml` - **Build Config** - Reusable GitHub Action that parses `scripts/implemented_variants.txt` for matrix CI builds
* `.gitmodules` - **Build Config** - Submodule configuration (vendor/hqc-ref pinning)

---

## Παράρτημα Γ: Ακρωνύμια και Συντομογραφίες

* `docs/thesis/preamble/glossary.tex` - **Documentation** - All acronym definitions via `\newabbreviation{}` for `\gls{}` usage
* `docs/thesis/appendices/acronyms.tex` - **Documentation** - Acronyms appendix that includes the rendered glossary

---

## Supplementary: Writing Drafts

| Αρχείο | Σχετικές Ενότητες | Περιεχόμενο |
|--------|-------------------|-------------|
| `writing/optimizations_overview.md` | §6–§7 overview, §8, presentation slides | Marp slides — condensed narrative of all optimizations with final numbers |
| `writing/reference_explanation.md` | §2.1–2.5 | Detailed vendor code walkthrough, HQC flow, module interactions |
| `writing/mult_alternatives.md` | §6.2.6, §9.1 | TC3/TC4/FAFFT/sparse analysis, measured regressions, "why Karatsuba wins" |
| `writing/VLEN_scaling.md` | §8.4 | Complete VLEN analysis with tables, ready for thesis text |
| `writing/compiler-comparison.md` | §5.2, §8.5, Appendix A | Full toolchain analysis, dual-compiler workflow, per-phase flags |
| `writing/zkt_ct.md` | §10.3 (μελλοντικό) | Zkt constant-time formal model, engineering diagnostics — deferred to future work |

---

## Supplementary: Experiments Registry

| Experiment | Status | Σχετικές Ενότητες | Σημασία |
|-----------|--------|-------------------|---------|
| `experiments/EXP-toomcook3-gf2x-zbc/` | In-Progress | §6.2.6 | TC3 with GF(4) evaluation — demonstrates why Karatsuba wins |
| `experiments/EXP-intrinsics-vs-asm/` | Empty/Planned | §3.5, §5.6 | Planned comparison of intrinsics vs hand-written assembly |
| `experiments/promoted/EXP-karatsuba-zbc-leaf-fixed/` | Promoted | §6.2.1, §6.2.4 | Fixed-size leaf optimization for scalar path (−3.21% polymul) |
| `experiments/promoted/EXP-decode-rs-zvbc/` | Promoted | §7.2.3–§7.2.4 | Zvbc RS decode with FFT root-finding (−84% decode instret) |
| `experiments/promoted/EXP-keccak-xor-bytes-rvv/` | Promoted | §7.1.4 | RVV byte-streaming for Keccak I/O (−3.48% total KEM) |
| `experiments/promoted/EXP-target2-bitmanip-scatter/` | Promoted | §7.3.2 | Bitmanip-accelerated scatter (−15.2% encaps) |
| `experiments/manifest.yaml` (EXP-mem-fastpath) | Promoted (manifest-only) | §5.5.2 | RVV memcpy/memmove fast paths promoted to `sim/common/mem.c` (−1.62% KEM) |
| `experiments/manifest.yaml` (EXP-keccak-scalar-zbb) | Data-extracted (code removed) | §7.1.1, §9.2.1 | Scalar Zbb ablation — explicit builtins match auto-lowering, ≤0.30% KEM impact |
| `experiments/manifest.yaml` (EXP-karatsuba-zbc-champion) | Promoted (baseline marker) | §6.2 | Records the scalar polymul baseline numbers per security level |
| `experiments/manifest.yaml` (EXP-karatsuba-zvbc-champion) | Promoted (baseline marker) | §6.4 | Records the Zvbc polymul baseline numbers per security level |
| `experiments/dropped/EXP-fafft-zvbc-towerfield/` | Dropped | §6.2.6, §9.1 | FAFFT tower-field incompatible with vclmul.vv (8.97× slower) |
| `experiments/dropped/EXP-fafft-rvv-blueprint/` | Dropped | §6.2.6 | FAFFT RVV — 4-5× regression |
| `experiments/dropped/archive_keccak_c4_register_wall/` | Dropped | §7.1.3 | 4-way Keccak — register spilling |
| `experiments/dropped/archive_keccak_x4_batching/` | Dropped | §7.1.3 | x4 batching — register pressure too high |
| `experiments/dropped/EXP-keccak-rvv-only-legacy/` | Archived | §7.1.2 | Base-RVV Keccak — RVV Paradox (7100 vs 4680 ref) |
| `experiments/dropped/EXP-keccak-rvv-x2-legacy/` | Archived | §7.1.2 | RVV x2 batching — identical to rvv_only |
| `experiments/dropped/EXP-keccak-zvbb-gather-legacy/` | Archived | §7.1.3 | Single-state Zvbb gather — superseded by x2 SoA (2838 vs 2114) |
| `experiments/dropped/EXP-zvbc-schoolbook/` | Dropped | §6.4 | Zvbc schoolbook without Karatsuba — suboptimal (178k vs 72k) |
| `experiments/dropped/EXP-decode-rs-fft-zvbc/` | Dropped | §7.2.4 | Earlier FFT Zvbc iteration — refined version promoted |
| `experiments/dropped/EXP-karatsuba-ablation/` | Dropped | §9.2 | Karatsuba ablation study archive |
| `experiments/dropped/EXP-karatsuba-ap2-specialized/` | Dropped | §6.2.3 | Pre-promotion AP2 specialized prototype |
| `experiments/dropped/EXP-karatsuba-leaf-v2-micro/` | Dropped | §6.2.4 | Leaf v2 micro-optimization variant superseded by leaf-fixed |
| `experiments/dropped/EXP-karatsuba-v2-a5/` | Dropped | §6.2.4 | Karatsuba v2 axis-A5 variant (superseded) |
| `experiments/dropped/archive_hybrid_zbc/` | Archived | §6.2 | Pre-AP2 hybrid thresholded recursion (incl. v2/v3 + key-cache header) |
| `experiments/dropped/archive_karatsuba_zbc_pre_ap2/` | Archived | §6.2.3 | Pre-AP2 dynamic recursion scalar path |
| `experiments/dropped/archive_karatsuba_zbc_exp/` | Archived | §6.2 | Experimental scalar Karatsuba variant snapshot |
| `experiments/dropped/archive_karatsuba_zbc_pre_leaffixed/` | Archived | §6.2.4 | Scalar path snapshot before leaf-fixed promotion |
| `experiments/dropped/archive_karatsuba_zbc_pre_nomemset/` | Archived | §6.2.2 | Scalar path snapshot before memset removal |
| `experiments/dropped/archive_karatsuba_zbc_pre_scalarsweep/` | Archived | §6.2.4 | Scalar path snapshot before threshold sweep |
| `experiments/dropped/archive_karatsuba_zbc_pre_target2/` | Archived | §7.3.2 | Scalar path + vector.c snapshot before scatter promotion |
| `experiments/dropped/archive_karatsuba_zbc_pre_target4/` | Archived | §6.2 | Scalar path snapshot before Target-4 work |
| `experiments/dropped/archive_karatsuba_zvbc_pre_ap2/` | Archived | §6.4 | Pre-AP2 dynamic recursion Zvbc path (with notes) |
| `experiments/dropped/archive_karatsuba_zvbc_prototypes/` | Archived | §6.4 | Early Zvbc leaf prototypes (v0/v1/v2) |
| `experiments/manifest.schema.json` | Schema | Β.1, Β.2 | JSON Schema enforcing manifest.yaml fields (used by `scripts/ci/validate_manifest.py`) |

---

## Supplementary: Key Data Artifacts

| Artifact Path | Σχετικές Ενότητες | Περιεχόμενο |
|--------------|-------------------|-------------|
| `analysis/benchmarks/suites/all_suite_2026-05-13_13-33/` | §8.2, §8.3 | Complete KEM suite: keypair/encaps/decaps/vect_mul CSVs |
| `analysis/benchmarks/vlen_sweep/` | §8.4 | VLEN sweep data for karatsuba_rvv/zvbc/ref_vector |
| `analysis/benchmarks/compiler_compare/` | §8.5 | GCC vs Clang instret per variant (scalar + vector subtrees) |
| `analysis/benchmarks/isolated/keccak/` | §8.1.2 | Canonical Keccak benchmark CSVs (ref and opt) |
| `analysis/benchmarks/isolated/decode/` | §8.1.3 | Canonical decode benchmark CSVs (ref, rvv, zvbc) |
| `analysis/profiles/histograms/` | §5.3, §9.2 | PC histogram data for instruction mix analysis (scalar/vector) |
| `analysis/profiles/traces/` | §5.3 | Spike execution traces for hotspot identification (scalar/vector) |
| `analysis/profiles/cpi/reports/` | §5.3, §9.1 | CPI model reports per security level |
| `analysis/profiles/cpi/scalar/` | §5.3 | CPI model inputs for scalar variants |
| `analysis/profiles/cpi/vector/` | §5.3 | CPI model inputs for vector variants |
| `analysis/profiles/cache/scalar/` | §5.3 | Cache simulation data for scalar variants |
| `analysis/profiles/cache/vector/` | §5.3 | Cache simulation data for vector variants |
| `analysis/profiles/memory/` | §5.3 | Memory access pattern profiles (scalar/vector) |
| `analysis/reports/scoreboards/` | §6.2.6 | Historical scoreboard tracking for polymul experiments (fafft v4–v6, fafft_zbc, hybrid v3, hybrid_zbc, polymul_zbc) |
| `analysis/reports/archive/` | §6–§8 | Archived intermediate profiling reports (task-level progression: karatsuba_zbc tasks 1–4, gather/x4 imv/imbv variants) |
| `docs/proposals/RESULTS/keccak_unrolling_results.md` | §7.1, §7.1.4 | Keccak unrolling experiment results |
| `docs/proposals/RESULTS/load64_intrinsic_results.md` | §7.1.4 | load64 intrinsic optimization results |
| `docs/proposals/RESULTS/reed_solomon_zvbc_results.md` | §7.2.3 | RS Zvbc optimization results |
| `docs/proposals/RESULTS/zbb_ablation_study.md` | §9.2.1 | Zbb ablation quantitative data |
| `docs/figures/cycle_counts.pdf` | §8 | Pre-rendered cycle-count figure for thesis import |
| `docs/figures/instruction_mix.pdf` | §5.3, §9.2 | Pre-rendered instruction-mix figure |
| `docs/figures/latest_dashboard.png` | §8 | Latest combined dashboard snapshot |

---

