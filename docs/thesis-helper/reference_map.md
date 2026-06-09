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
4. [Κεφάλαιο 4: Σχετική Βιβλιογραφία (Related Work)](#κεφάλαιο-4-σχετική-βιβλιογραφία-related-work)
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

* `docs/thesis-helper/notes/PQC-intro.txt` - **Research Notes** - Contains "Harvest now, Decrypt later" strategy, CNSA 2.0 mandate, and Shor's algorithm threat model
* `docs/thesis/chapters/01-introduction.tex` - **Writing Draft** - LaTeX source for Chapter 1
* `docs/thesis/figures/ch1-introduction/timeline-pqc.tex` - **Figure** - TikZ timeline diagram: classical crypto → quantum threat → NIST competition → standardization

### §1.2 Το Πλαίσιο Τυποποίησης του NIST

* `docs/thesis-helper/notes/PQC-Algorithms.txt` - **Research Notes** - NIST standardization rounds overview, HQC selection timeline, "Επιλέχθηκε για standardization 11/03/2025"
* `docs/thesis/figures/ch1-introduction/nist-rounds-flowchart.tex` - **Figure** - TikZ flowchart NIST rounds: Round 1 (69) → Round 2 (26) → Round 3 (7) → Round 4 (4) → Final (HQC)

### §1.3.1 Κρυπτογραφική Ευελιξία ως Απαίτηση

* `docs/thesis-helper/notes/PQC-intro.txt` - **Research Notes** - SIKE failure case study ("SIKE horror story"), crypto agility arguments, "Problem 1 - Crypto Agility is Destroyed"

### §1.3.2 Επιτάχυνση μέσω ISA Extensions σε Ανοιχτή Αρχιτεκτονική

* `docs/thesis-helper/notes/PQC-intro.txt` - **Research Notes** - RISC-V philosophy of ISA extensions replacing fixed-function accelerators
* `docs/thesis-helper/notes/notes_mine.md` - **Research Notes** - "Zbb, V, Zvbb part of the RVA23 standard" — mainstream adoption path evidence
* `docs/thesis/figures/ch1-introduction/acceleration-spectrum.tex` - **Figure** - TikZ diagram "spectrum of acceleration": Fixed ASIC ← FPGA ← ISA Extensions ← Pure Software

### §1.4 Σκοπός και Συνεισφορά της Διατριβής

* `docs/thesis-helper/notes/PQC-Algorithms.txt` - **Research Notes** - HQC vs alternative PQC algorithms comparison
* `docs/thesis-helper/notes/notes_detailed.md` (Discovery 3) - **Research Notes** - Summary speedup table establishing the contribution claims
* `docs/references/markdown/riscv-other-algorithms/1-eco-crystals-rv64imb.md` - **Literature** - ECO-CRYSTALS: demonstrates RISC-V PQC optimization is active area but only for lattice-based (Kyber/Dilithium), not code-based HQC
* `docs/references/markdown/riscv-other-algorithms/2-rv3264imbrvv.md` - **Literature** - Zhang et al. TCHES 2025: most comprehensive RISC-V PQC work — covers all ISA combos but only lattice-based, reinforcing the code-based gap

### §1.5 Δομή της Εργασίας

* `docs/thesis-helper/sections.md` - **Documentation** - Authoritative thesis outline with per-section content descriptions; use as the structural reference for the roadmap paragraph
* `docs/thesis/figures/ch1-introduction/thesis-at-a-glance.tex` - **Figure** - TikZ infographic "thesis at a glance": problem → approach → key results

---

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο Αλγορίθμων — HQC

### §2.1 Code-Based Κρυπτογραφία

#### §2.1.1 Μαθηματική Θεμελίωση

* `docs/thesis-helper/notes/PQC-Algorithms.txt` - **Research Notes** - "HQC Intro", security assumptions, code-based crypto foundations
* `docs/thesis-helper/writing/reference_explanation.md` - **Writing Draft** - Detailed vendor code walkthrough explaining HQC mathematical foundations
* `docs/thesis/chapters/02-theoretical-background.tex` - **Writing Draft** - LaTeX source for Chapter 2

#### §2.1.2 Ιστορικό Πλαίσιο

* `docs/thesis-helper/notes/PQC-Algorithms.txt` - **Research Notes** - McEliece genealogy, quasi-cyclic MDPC evolution, HQC positioning vs BIKE/Classic McEliece
* `docs/thesis/figures/ch2-background/code-based-genealogy.tex` - **Figure** - TikZ genealogy diagram: McEliece → Niederreiter → MDPC → QC-MDPC → HQC

### §2.2 Ο Αλγόριθμος HQC

#### §2.2.1 Ομάδα Σχεδιασμού και Πορεία Τυποποίησης

* `docs/thesis-helper/notes/PQC-Algorithms.txt` - **Research Notes** - NIST round progression, sparse→dense spec change (2024), security parameters table
* `vendor/hqc-ref/CHANGELOG` - **Reference Code** - Upstream HQC changelog showing spec revisions across NIST rounds
* `docs/thesis/figures/ch2-background/hqc-timeline.tex` - **Figure** - TikZ HQC timeline: submission (2017) → rounds → spec change (2024) → Final
* `docs/thesis/figures/ch2-background/hqc-params-table.tex` - **Figure** - LaTeX table of HQC parameters per security level (N, n₁, n₂, ω, δ)

#### §2.2.2 Λειτουργία KEM: KeyGen, Encaps, Decaps

* `docs/thesis-helper/writing/reference_explanation.md` - **Writing Draft** - "Phase 6: The Core Scheme", KEM flow pseudocode
* `src/hqc.c` (`hqc_pke_keygen`, `hqc_pke_encrypt`, `hqc_pke_decrypt`) - **Source Code** - Actual implementation of the three KEM operations showing polynomial operations, hash calls, and FO transform re-encryption check
* `vendor/hqc-ref/src/common/kem.c` - **Reference Code** - Vendor IND-CCA2 KEM wrapper (Encaps/Decaps + FO transform) used as the unmodified algorithmic baseline
* `vendor/hqc-ref/src/ref/hqc.c` - **Reference Code** - Vendor PKE primitives (keygen / encrypt / decrypt) underlying the KEM
* `docs/thesis/figures/ch2-background/hqc-kem-sequence.tex` - **Figure** - TikZ sequence diagram: Alice/Bob key exchange (KeyGen → Encaps → Decaps)

#### §2.2.3 Υπολογιστική Δομή και Κατανομή Κόστους

* `docs/thesis-helper/notes/notes_detailed.md` (Discovery 6) - **Research Notes** - "polynomial multiplication was originally about 96% of decaps", pie chart data source
* `src/hqc.c` - **Source Code** - Shows which kernels (vect_mul, hash_*, code_decode) are called in each KEM operation
* `scripts/report/generate_charts.py` - **Script** - Generates PDF pie charts and bar charts from benchmark CSVs; produces the §2.2.3 cost distribution pie chart (absorbed former generate_pie_charts.py)
* `results/simulation/profiling/traces/scalar_ref/` - **Data Report** - Trace hotspot CSVs consumed by the chart generator
* `results/reports/figures/pie_scalar-ref.pdf` - **Figure** - Pre-rendered pie chart of reference scalar HQC instruction breakdown (polymul ~96%, Keccak, decode, misc)
* `results/reports/figures/pie_combined.pdf` - **Figure** - Combined pie chart comparison across variants
* `docs/thesis/figures/ch2-background/hqc-dataflow.tex` - **Figure** - TikZ block diagram: dataflow Keygen/Encaps/Decaps with color-coded kernels (polymul=red, Keccak=orange, decode=yellow)

### §2.3 Πολυωνυμικός Πολλαπλασιασμός σε GF(2)[x]

* `docs/thesis-helper/optimizations/multiplication.md` §1 - **Optimization Notes** - "Baseline Architecture and Starting Point", GF(2)[x] ring definition, Karatsuba mathematical theory
* `vendor/hqc-ref/src/ref/gf2x.c` - **Reference Code** - Vendor bit-serial GF(2)[x] multiplication used as the mathematical reference implementation
* `docs/thesis/figures/ch2-background/karatsuba-recursion-tree.tex` - **Figure** - TikZ recursion tree diagram (2 levels): a·b → {aL·bL, aH·bH, (aL⊕aH)·(bL⊕bH)}
* `docs/thesis/figures/ch2-background/karatsuba-complexity.tex` - **Figure** - TikZ complexity comparison: schoolbook O(n²) vs Karatsuba O(n^1.585)

### §2.4 Συναρτήσεις Κατακερματισμού SHA-3 / Keccak

* `docs/thesis-helper/optimizations/keccak.md` §1 - **Optimization Notes** - "Baseline Architecture", Sponge Construction, Keccak-f[1600] round structure (θ, ρ, π, χ, ι)
* `vendor/hqc-ref/lib/fips202/fips202.c` - **Reference Code** - Vendor FIPS-202 SHA-3 / SHAKE reference (Keccak-f[1600] permutation, 5-step round, sponge absorb/squeeze)
* `vendor/hqc-ref/lib/fips202/fips202.h` - **Reference Code** - FIPS-202 public API declarations (SHAKE-256 incremental, SHA3-256/512)
* `docs/thesis/figures/ch2-background/keccak-sponge.tex` - **Figure** - TikZ sponge construction diagram: absorb phase → squeeze phase
* `docs/thesis/figures/ch2-background/keccak-state-3d.tex` - **Figure** - TikZ 3D visualization of 5×5×64 Keccak state (lanes, columns, planes)
* `docs/thesis/figures/ch2-background/keccak-steps-table.tex` - **Figure** - LaTeX table of 5 Keccak steps (θ, ρ, π, χ, ι) with formulas

### §2.5 Κώδικες Διόρθωσης Σφαλμάτων (Reed-Muller, Reed-Solomon)

* `docs/thesis-helper/optimizations/decode.md` §1 - **Optimization Notes** - "Baseline Architecture", RM(1,7), RS over GF(2^8), concatenated code pipeline, Berlekamp-Massey theory
* `vendor/hqc-ref/src/ref/reed_muller.c` - **Reference Code** - Vendor scalar Reed-Muller decoder (Hadamard transform + peak find) baseline
* `vendor/hqc-ref/src/ref/reed_solomon.c` - **Reference Code** - Vendor scalar Reed-Solomon decoder (syndromes / Berlekamp-Massey / Chien / Forney) baseline
* `vendor/hqc-ref/src/common/code.c` - **Reference Code** - Vendor concatenated decode glue: orchestrates RM (inner) + RS (outer) pipeline
* `vendor/hqc-ref/src/common/fft.c` - **Reference Code** - Vendor recursive Gao-Mateer additive FFT (`fft_rec`) — baseline for §7.2.4 root-finding rewrite
* `docs/thesis/figures/ch2-background/concatenated-pipeline.tex` - **Figure** - TikZ concatenated decoding pipeline: received_word → RM decode → RS decode → corrected output
* `docs/thesis/figures/ch2-background/rs-decode-pipeline.tex` - **Figure** - TikZ RS decoding flowchart: Syndromes → BM → Root-finding → Forney → Correction

---

## Κεφάλαιο 3: Θεωρητικό Υπόβαθρο Αρχιτεκτονικής — RISC-V και Κρυπτογραφικές Επεκτάσεις

### §3.1 Σχεδιαστικές Αρχές και Βασική Αρχιτεκτονική RV64

#### §3.1.1 Ιστορικό Πλαίσιο και Φιλοσοφία

* `docs/thesis-helper/notes/notes_mine.md` - **Research Notes** - RVA23 profile mandatory extensions, mainstream adoption evidence
* `docs/thesis/chapters/03-risc-v-architecture.tex` - **Writing Draft** - LaTeX source for Chapter 3

#### §3.1.2 Βασικά Χαρακτηριστικά RV64I

* `docs/thesis/figures/ch3-riscv/rv64-instruction-formats.tex` - **Figure** - TikZ diagram of 6 instruction formats (R/I/S/B/U/J) with bit fields

#### §3.1.3 Σύμβαση Ονοματοδοσίας Επεκτάσεων και Standard Profiles

* `docs/thesis-helper/notes/notes_mine.md` - **Research Notes** - RVA23 profile mandatory extensions, mainstream adoption evidence
* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Concrete ISA strings used: `rv64gc_zbc_zbb_zbs`, `rv64gcv_zbc_zbb_zba_zvbc_zvbb` — demonstrates modular extension composition

#### §3.1.4 Σύγκριση Σχεδιαστικής Φιλοσοφίας με ARM και x86

* `docs/thesis-helper/notes/notes_mine.md` - **Research Notes** - "Architectural Comparison: SHA-2 vs SHA-3", ARM EOR3/RAX1 vs RISC-V Zvbb comparison

### §3.2 Scalar Κρυπτογραφικές Επεκτάσεις (Zbc, Zbb, Zbkb)

* `docs/thesis-helper/optimizations/multiplication.md` §3.2.1 - **Optimization Notes** - "Native clmul/clmulh Injection", Zbc description and before/after analysis
* `src/common/gf.c` (`gf_mul`, `gf_square`) - **Source Code** - Example of Zbc usage via `__riscv_clmul_64` intrinsic (implementation details in §6.2)
* `src/mult/karatsuba_zbc/gf2x_clmul.c` (`clmul_leaf`) - **Source Code** - Example of clmul/clmulh pair usage (implementation details in §6.2)
* `docs/thesis/figures/ch3-riscv/clmul-before-after.tex` - **Figure** - LaTeX before/after code listing: shift-XOR loop vs clmul+clmulh
* `docs/thesis/figures/ch3-riscv/clmul-semantics.tex` - **Figure** - TikZ diagram showing carry-less multiply semantics at bit level

### §3.3 Αρχιτεκτονική Διανυσματικής Μονάδας (RVV 1.0)

#### §3.3.1 Γενική Αρχιτεκτονική και Vector-Length Agnosticism

* `docs/thesis-helper/writing/VLEN_scaling.md` - **Writing Draft** - VLEN, LMUL, VLA concepts with concrete benchmark data
* `docs/thesis/figures/ch3-riscv/rvv-register-file.tex` - **Figure** - TikZ diagram: v0–v31 vector register file, VLEN bits wide

#### §3.3.2 Control/Status Registers και Παραμετροποίηση

* `docs/thesis-helper/writing/VLEN_scaling.md` "Instruction Budget Breakdown" - **Writing Draft** - VLMAX formula, SEW/LMUL interaction
* `docs/thesis/figures/ch3-riscv/rvv-vlmax-table.tex` - **Figure** - LaTeX table of VLMAX for various VLEN/SEW/LMUL combinations

#### §3.3.3 Strip-Mining και ο Μηχανισμός vsetvl

* `sim/common/mem.c` (`memcpy`) - **Source Code** - Minimal worked example of RVV strip-mining loop: vsetvl + vle8 + vse8 pattern
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`rvv_xor3_u64`) - **Source Code** - Clean VLA helper demonstrating strip-mining with SEW=64, LMUL=1
* `docs/thesis/figures/ch3-riscv/rvv-strip-mining.tex` - **Figure** - TikZ strip-mining visualization: same code, different VLMAX → different iteration counts

#### §3.3.4 Tail/Mask Policies και Κατηγορίες Εντολών

* `docs/thesis/figures/ch3-riscv/rvv-lmul-grouping.tex` - **Figure** - TikZ LMUL grouping diagram: register file color-coded by group
* `docs/thesis/figures/ch3-riscv/rvv-instruction-categories.tex` - **Figure** - LaTeX table of RVV instruction categories with examples and thesis usage

#### §3.3.5 Motivating Example — XOR Loop σε RVV

* `src/vector.c` (`vect_add`) - **Source Code** - RVV XOR loop: strip-mined vxor of two polynomial vectors — direct motivating example
* `docs/thesis/figures/ch3-riscv/rvv-xor-example.tex` - **Figure** - LaTeX side-by-side: C scalar loop vs RVV intrinsics with annotated arrows

### §3.4 Διανυσματικές Κρυπτογραφικές Επεκτάσεις (Zvbc, Zvbb)

* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`vclmul_leaf`) - **Source Code** - Demonstrates vclmul.vv/vclmulh.vv usage for vectorized carry-less multiply
* `src/keccak/fips202_zvbb_x2.c` (`KeccakF1600_StatePermute_x2`) - **Source Code** - Demonstrates vror.vv/vandn.vv (Zvbb) usage for vectorized Keccak rotations and χ step
* `vendor/rvv-keccak/fips202_rv64imbv_hybrid_x4.S` - **Reference Code** - Third-party RVV+Zvbb (imbv) Keccak x4 hybrid assembly reference
* `vendor/rvv-keccak/fips202_rv64imv_hybrid_x4.S` - **Reference Code** - Third-party base-RVV (imv, no Zvbb) Keccak x4 hybrid assembly — counterpart used to isolate Zvbb's contribution
* `vendor/rvv-keccak/fips202x_rv64.c` - **Reference Code** - C glue around the assembly cores; exposes batched SHA3/SHAKE APIs (x2/x3/x4) consumed by external benchmarks
* `vendor/rvv-keccak/fips202x.h` - **Reference Code** - Public API + macros for the rvv-keccak batched implementations
* `docs/thesis/figures/ch3-riscv/zvbc-zvbb-before-after.tex` - **Figure** - LaTeX before/after table: operations with/without Zvbc/Zvbb

### §3.5 Μέθοδοι Προγραμματισμού RISC-V — Φάσμα Αφαίρεσης

* `sim/common/startup.S` - **Source Code** - Pure assembly example: bare-metal boot, CSR manipulation, vector unit enable
* `src/common/bench.c` (`hqc_rdcycle`, `hqc_rdinstret`) - **Source Code** - Inline assembly example: `__asm__ volatile("csrr %0, 0xb00")` for CSR reads
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` - **Source Code** - C intrinsics example: `__riscv_vclmul_vv_u64m1()`, `__riscv_vsetvl_e64m1()`
* `src/common/gf.c` (`gf_mul`) - **Source Code** - C intrinsics example: `__riscv_clmul_64()` scalar Zbc intrinsic
* `src/vector.c` (`vect_add`) - **Source Code** - Auto-Lowering example: plain C with Zba/Zbs auto-lowered by compiler
* `docs/thesis/figures/ch3-riscv/riscv-programming-spectrum.tex` - **Figure** - TikZ horizontal spectrum: Pure .S ←→ Inline Asm ←→ Intrinsics ←→ Auto-Lowering
* `docs/thesis/figures/ch3-riscv/programming-methods-table.tex` - **Figure** - LaTeX comparison table of 4 methods: control, optimization, portability, error-proneness
* `docs/thesis/figures/ch3-riscv/inline-asm-vs-intrinsic.tex` - **Figure** - LaTeX side-by-side: inline asm clmul vs intrinsic clmul

---

## Κεφάλαιο 9: Συζήτηση και Ερμηνεία Αποτελεσμάτων

### §9.1 Σύγκριση με Ετερογενείς Αρχιτεκτονικές

* `docs/thesis-helper/notes/notes_detailed.md` §5.3.1–5.3.2 - **Research Notes** - Cross-architecture table (AVX-512, Zen4, M4, FAFFT, ours), instret vs cycles methodology
* `docs/METHODOLOGY.md` §11, §12 - **Documentation** - "We do NOT claim wall-clock time on real hardware", Spike limitations
* `docs/thesis-helper/writing/mult_alternatives.md` - **Writing Draft** - FAFFT O(n log n) vs AP-2 Karatsuba comparison, tower-field incompatibility with vclmul.vv
* `docs/references/notes/HQC Optimization Benchmarks Research.txt` - **Literature** - Aggregated cross-platform benchmark numbers
* `results/simulation/profiling/cpi/reports/` - **Data Report** - CPI-derived cycle estimates supporting cross-architecture cycle-equivalent comparisons
* `results/reports/summaries/speedups_cross_platform.md` - **Data Report** - Cross-platform speedup comparison summary
* `results/reports/figures/speedup_cross_platform.pdf` - **Figure** - Cross-architecture comparison chart
* `docs/thesis/chapters/09-discussion.tex` - **Writing Draft** - LaTeX source for Chapter 9

### §9.2 ISA Ablation Study

#### §9.2.1 Ποσοτική Συνεισφορά Κάθε Extension

* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Ablation ISA variable: set to rv64gc_zbc to strip Zbb and measure impact
* `docs/thesis-helper/notes/notes_detailed.md` (Discovery 5) - **Research Notes** - "Zbb alone yields 37% Keccak reduction via native rori"
* `docs/thesis-helper/notes/notes_detailed.md` (Discovery 4) - **Research Notes** - "ref_vector vs ref_full_crypto — negligible difference", compiler ceiling
* `docs/proposals/RESULTS/zbb_ablation_study.md` - **Data Report** - Zbb ablation quantitative results per security level
* `results/reports/figures/waterfall_ablation.pdf` - **Figure** - Waterfall chart: baseline → +Zbc → +Zbb → +RVV → +Zvbc → +Zvbb = final

#### §9.2.2 Zba Address-Generation — Αποτελέσματα Ablation

* `docs/thesis-helper/optimizations/zba_ablation.md` - **Optimization Notes** - Full Zba ablation data: −5.6% zvbc polymul, −3.3% zvbc KEM, −1.0% rvv, 0% scalar

#### §9.2.3 Zbs Single-Bit — Αποτελέσματα Ablation

* `src/vector.c` (`vect_write_support_to_vector`) - **Source Code** - Primary Zbs beneficiary: bit-scatter loop where `bset` replaces li+sll+or
* `cmake/options.cmake` (HQC_SCALAR_MARCH) - **Build Config** - Zbs in scalar march only (rv64gc_zbc_zbb_zbs), not in vector variants

#### §9.2.4 Compiler Auto-Lowering — Όρια και Ευρήματα

* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` - **Source Code** - Evidence that compiler does NOT auto-emit vclmul.vv from generic C — requires explicit intrinsics
* `src/keccak/fips202_zvbb_x2.c` - **Source Code** - Evidence that compiler does NOT auto-emit vror.vv from generic C — requires explicit intrinsics
* `scripts/ci/check_opcodes.sh` - **Script** - Backing opcode-verification script invoked by the CI workflow
* `scripts/ci/check_isa_string.sh` - **Script** - ISA string verification ensuring correct -march flags per variant

### §9.3 Code Size vs Performance Trade-off

* `scripts/gen/gen_karatsuba_specialized.py` - **Script** - Generator controls unroll depth; different depths produce different .text sizes
* `results/reports/variants/scalar_zb_L128_report.md` - **Data Report** - Contains .text size data for scalar fully-unrolled path
* `results/reports/variants/vector_zvk_L128_report.md` - **Data Report** - Contains .text size data for vector path (compact VLA helpers)
* `scripts/experiments/run_hybrid_depth_sweep.sh` - **Script** - Hybrid recursion depth sweep used to scan the depth × instret × .text trade-off
* `results/reports/figures/scatter_code_size.pdf` - **Figure** - Scatter plot: .text size vs instret per variant/configuration

### §9.4 Σχεδιαστικές Αρχές που Γενικεύονται

* `docs/thesis-helper/optimizations/multiplication.md` - **Optimization Notes** - General principles: carry-less multiply efficiency, VLA strip-mining value
* `docs/thesis-helper/optimizations/keccak.md` - **Optimization Notes** - ISA completeness > ISA width principle
* `scripts/gen/gen_karatsuba_specialized.py` - **Script** - Metaprogramming vs hand-tuning: code generation approach

### §9.5 Συγκριτική Αξιολόγηση Μεταγλωττιστών (Clang vs GCC)

* `docs/thesis-helper/writing/compiler-comparison.md` - **Writing Draft** - Full GCC vs Clang codegen analysis, dual-compiler workflow, GCC Zvbb incompatibility
* `results/simulation/benchmarks/compiler_compare/` - **Data Report** - Compiler comparison CSVs for scalar and vector variants
* `results/reports/summaries/compiler_analysis.md` - **Data Report** - Generated compiler showdown report with winner/margin per variant
* `scripts/report/generate_compiler_compare.py` - **Script** - Generates compiler comparison Markdown from CSV data
* `results/reports/figures/bar_compiler_comparison.pdf` - **Figure** - Side-by-side bar chart: Clang vs GCC instret per kernel and tier

### §9.6 Επαλήθευση Σταθερού Χρόνου Εκτέλεσης (Constant-Time Verification)

#### §9.6.1 Μοντέλο Απειλής και Ορισμός Constant-Time

* `docs/thesis-helper/writing/constant_time_verification.md` §1 - **Writing Draft** - Threat model, CT definition, Zkt subset, component-by-component CT analysis
* `docs/thesis-helper/writing/zkt_ct.md` - **Writing Draft** - Zkt constant-time formal model, Zkt-compliant instruction subset

#### §9.6.2 Σχεδιαστικά Πρότυπα για CT GF(2)[X] Αριθμητική

* `docs/thesis-helper/writing/constant_time_verification.md` §2 - **Writing Draft** - CT design patterns: clmul hardware guarantee, Karatsuba deterministic recursion, leaf specialization, RVV fixed-config operations
* `src/mult/karatsuba_zbc/gf2x_clmul.c` (`clmul_leaf`) - **Source Code** - Fully unrolled leaf with zero branches — CT by construction
* `src/mult/karatsuba_zvbc/gf2x_vclmul.c` (`vclmul_leaf`) - **Source Code** - Vector leaf with fixed LMUL/SEW, no predicated operations on secrets

#### §9.6.3 Στατική Επαλήθευση — ctgrind (Valgrind Memcheck)

* `docs/thesis-helper/writing/constant_time_verification.md` §3 - **Writing Draft** - ctgrind methodology, integration, limitations
* `tests/ct/ct_harness.c` - **Source Code** - Constant-time verification harness: marks sigma as UNDEFINED, exercises crypto_kem_dec()
* `results/ct_verify/valgrind_ct/` - **Data Report** - Ctgrind XML results for various variants (karatsuba_ref, HQC-128/192/256)
* `scripts/ci/check_ct.sh` - **Script** - CI script for automated constant-time verification via ctgrind
* `.github/workflows/ct-verify.yml` - **Build Config** - CI workflow running ctgrind on every push to main

#### §9.6.4 Εμπειρική Επαλήθευση σε Hardware — Dudect (BPI-F3)

* `docs/thesis-helper/writing/constant_time_verification.md` §4 - **Writing Draft** - Full dudect methodology: Welch's t-test, prepare-then-measure, warm-up, percentile cropping, multi-run protocol
* `tests/ct_dudect/dudect_polymul.c` - **Source Code** - Dudect harness for isolated vect_mul() timing verification
* `tests/ct_dudect/dudect_kem.c` - **Source Code** - Dudect harness for full crypto_kem_dec() end-to-end timing verification
* `tests/ct_dudect/dudect_ttest.h` - **Source Code** - Welch's t-test accumulator: online mean/variance computation, first-order and second-order (percentile-cropped) statistics
* `tests/ct_dudect/dudect_percentile.h` - **Source Code** - Quickselect-based percentile cropping for second-order leakage detection
* `tests/ct_dudect/dudect_prng.h` - **Source Code** - Lightweight PRNG for dudect class assignment and random input generation
* `tests/ct_dudect/README.md` - **Documentation** - Dudect test suite documentation: build instructions, parameters, interpretation guide
* `tests/ct_dudect/run_multi.sh` - **Script** - Multi-run wrapper: N independent trials with fresh seeds, produces summary CSV for publication-grade evidence
* `scripts/test/run_ct_verify.sh` - **Script** - Unified CT verification runner: builds and executes dudect for full variant matrix (target × level × polymul), multi-run trials, summary table with verdicts
* `Makefile.hw` - **Build Config** - Hardware build system supporting SUITE=dudect target for native BPI-F3 compilation

#### §9.6.5 Στόχοι Δοκιμών και Πίνακας Variants

* `docs/thesis-helper/writing/constant_time_verification.md` §4.5, §4.10 - **Writing Draft** - Test target justification: why polymul and KEM are tested, why decode and Keccak are excluded

#### §9.6.6 Μεθοδολογικά Ευρήματα — Cache-State Bias και False Positives

* `docs/thesis-helper/writing/constant_time_verification.md` §8 - **Writing Draft** - Cache-state bias discovery, false positive analysis, prepare-then-measure resolution

#### §9.6.7 Πειραματικά Αποτελέσματα

* `docs/thesis-helper/writing/constant_time_verification.md` §5 - **Writing Draft** - Experimental results tables: t₁/t₂ values per variant/level/target
* `results/ct_verify/dudect/` - **Data Report** - Dudect timing analysis results: per-variant summary CSVs with t-statistics and verdicts

#### §9.6.8 Γιατί Τυπική Επαλήθευση Δεν Είναι Εφικτή για RVV/Zvbc

* `docs/thesis-helper/writing/constant_time_verification.md` §7 - **Writing Draft** - Formal verification landscape (Binsec/Rel, ct-verif, HolBA, Bedrock2), why they cannot verify RVV/Zvbc, Valgrind RISCV64 limitations

#### §9.6.9 Συμπληρωματικότητα Στρωμάτων Επαλήθευσης

* `docs/thesis-helper/writing/constant_time_verification.md` §6 - **Writing Draft** - Complementarity table: ctgrind vs dudect detection capabilities

#### §9.6.10 Ερμηνεία Αποτελεσμάτων

* `docs/thesis-helper/writing/constant_time_verification.md` §9 - **Writing Draft** - Interpretation caveats: dudect does not prove absence, sensitivity thresholds, multi-run strengthening

---

## Κεφάλαιο 10: Συμπεράσματα και Μελλοντικές Επεκτάσεις

### §10.1 Σύνοψη Συνεισφοράς

* `docs/thesis-helper/notes/notes_detailed.md` (Discovery 3) - **Research Notes** - Summary speedup table (×91, ×118, ×255 vect_mul; ×28, ×41, ×63 decaps)
* `results/reports/summaries/speedups_simulation.md` - **Data Report** - Final speedup numbers for thesis conclusion
* `results/reports/summaries/speedups_hardware.md` - **Data Report** - Hardware-validated speedup numbers
* `docs/proposals/final_optim.md` - **Documentation** - Senior-author analysis of remaining ratified extensions and their (rejected/promoted) verdicts
* `docs/thesis/chapters/10-conclusions.tex` - **Writing Draft** - LaTeX source for Chapter 10

### §10.2 Περιορισμοί

* `docs/METHODOLOGY.md` §11, §12 - **Documentation** - "We do NOT claim wall-clock time on real hardware", Spike limitations, CPI dilemma
* `docs/hardware/RTL/riscv_zvbc_zvbb_targets.md` - **Documentation** - Available RISC-V hardware targets with Zvbc/Zvbb support status
* `docs/hardware/BPI-F3/SSH_SETUP.md` - **Documentation** - BananaPi BPI-F3 (SpacemiT K1) as native hardware target

### §10.3 Μελλοντική Έρευνα

* `docs/thesis-helper/writing/zkt_ct.md` - **Writing Draft** - Zkt constant-time formal model, engineering diagnostics — formal verification as future work
* `docs/thesis-helper/notes/notes_mine.md` - **Research Notes** - "Future of RISC-V & SHA-3", PQC Task Group Keccak instructions
* `sim/rtl-marian/README.md` - **Documentation** - RTL soft-core (Marian) integration notes for future FPGA work
* `docs/hardware/RTL/marian_deployment_guide.md` - **Documentation** - Marian RTL deployment guide for cycle-accurate Zvbc simulation

---

## Παράρτημα Α: Οδηγός Εγκατάστασης Περιβάλλοντος

### Α.1 Μεταγλώττιση του RISC-V Toolchain

* `toolchain/build-gcc-sysroot.sh` - **Script** - GCC cross-compiler build script with sysroot configuration
* `toolchain/build-llvm.sh` - **Script** - LLVM/Clang cross-compiler build script for RISC-V target
* `docs/thesis-helper/writing/compiler-comparison.md` §3 - **Writing Draft** - Build steps documentation, unified riscv-gnu-toolchain
* `.github/actions/setup-riscv-env/action.yml` - **Build Config** - Reusable GitHub Action that installs the RISC-V toolchain + Spike — canonical environment recipe
* `docs/thesis/appendices/appendix-a.tex` - **Writing Draft** - LaTeX source for Appendix A

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

* `scripts/bench/run_bench.sh` - **Script** - Unified benchmark runner: mul/kem/keccak/decode/suite modes with Spike
* `scripts/bench/run_profile.sh` - **Script** - Profiling entry point: cache/memory/trace/histogram modes
* `scripts/bench/lib_bench.sh` - **Script** - Shared benchmark library: variant auto-mapping, track detection, ISA string resolution
* `scripts/bench/run_sweep.sh` - **Script** - VLEN sweep runner: iterates over VLEN values for scaling analysis
* `scripts/report/run_full_analysis.sh` - **Script** - Full analysis pipeline: runs all benchmarks and generates reports
* `CMakeLists.txt` - **Build Config** - Top-level CMake configuration for all variants and security levels
* `cmake/hqc_core.cmake` - **Build Config** - Core HQC library CMake module: source file lists, compile definitions, include paths
* `cmake/options.cmake` - **Build Config** - User-tunable build options (variant selection, security level, march, LTO/PGO)
* `tests/CMakeLists.txt` - **Build Config** - Test target definitions (KAT, bench, CT harness)
* `tests/unit/CMakeLists.txt` - **Build Config** - Unit-test target definitions
* `.github/actions/parse-variants/action.yml` - **Build Config** - Reusable GitHub Action that parses `scripts/implemented_variants.txt` for matrix CI builds
* `.gitmodules` - **Build Config** - Submodule configuration (vendor/hqc-ref pinning)
* `Makefile.hw` - **Build Config** - Hardware build system for native BPI-F3 compilation (bench, dudect, KAT targets)

---

## Παράρτημα Γ: Ακρωνύμια και Συντομογραφίες

* `docs/thesis/preamble/glossary.tex` - **Documentation** - All acronym definitions via `\newabbreviation{}` for `\gls{}` usage
* `docs/thesis/appendices/acronyms.tex` - **Documentation** - Acronyms appendix that includes the rendered glossary

---

## Supplementary: Writing Drafts

| Αρχείο | Σχετικές Ενότητες | Περιεχόμενο |
|--------|-------------------|-------------|
| `docs/thesis-helper/writing/optimizations_overview.md` | §6–§7 overview, §8, presentation slides | Marp slides — condensed narrative of all optimizations with final numbers |
| `docs/thesis-helper/writing/reference_explanation.md` | §2.1–2.5 | Detailed vendor code walkthrough, HQC flow, module interactions |
| `docs/thesis-helper/writing/mult_alternatives.md` | §6.2.6, §9.1 | TC3/TC4/FAFFT/sparse analysis, measured regressions, "why Karatsuba wins" |
| `docs/thesis-helper/writing/VLEN_scaling.md` | §8.4 | Complete VLEN analysis with tables, ready for thesis text |
| `docs/thesis-helper/writing/compiler-comparison.md` | §5.2, §9.5, Appendix A | Full toolchain analysis, dual-compiler workflow, per-phase flags |
| `docs/thesis-helper/writing/zkt_ct.md` | §9.6.1, §10.3 | Zkt constant-time formal model, engineering diagnostics — formal verification as future work |
| `docs/thesis-helper/writing/constant_time_verification.md` | §9.6 | Full CT verification methodology and results: threat model, ctgrind, dudect on BPI-F3, experimental results, formal verification gap analysis |
| `docs/thesis/chapters/01-introduction.tex` | §1 | LaTeX source for Chapter 1 |
| `docs/thesis/chapters/02-theoretical-background.tex` | §2 | LaTeX source for Chapter 2 |
| `docs/thesis/chapters/03-risc-v-architecture.tex` | §3 | LaTeX source for Chapter 3 |
| `docs/thesis/chapters/04-related-work.tex` | §4 | LaTeX source for Chapter 4 |
| `docs/thesis/chapters/05-experimental-setup.tex` | §5 | LaTeX source for Chapter 5 |
| `docs/thesis/chapters/06-polymul-optimization.tex` | §6 | LaTeX source for Chapter 6 |
| `docs/thesis/chapters/07-kernel-optimization.tex` | §7 | LaTeX source for Chapter 7 (main) |
| `docs/thesis/chapters/07-decode-section.tex` | §7.2 | LaTeX source for decode section of Chapter 7 |
| `docs/thesis/chapters/08-results.tex` | §8 | LaTeX source for Chapter 8 |
| `docs/thesis/chapters/09-discussion.tex` | §9 | LaTeX source for Chapter 9 |
| `docs/thesis/chapters/10-conclusions.tex` | §10 | LaTeX source for Chapter 10 |
| `docs/thesis/appendices/appendix-a.tex` | Appendix A | LaTeX source for Appendix A |
| `docs/thesis-helper/Corrections.txt` | All | Corrections and reviewer feedback notes |

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
| `results/simulation/benchmarks/kem/` | §8.2, §8.3 | Complete KEM suite: keypair/encaps/decaps/vect_mul CSVs |
| `results/simulation/benchmarks/vlen_scaling/` | §8.4 | VLEN sweep data for karatsuba_rvv/zvbc/ref_vector |
| `results/simulation/benchmarks/compiler_compare/` | §9.5 | GCC vs Clang instret per variant (scalar + vector subtrees) |
| `results/simulation/benchmarks/components/keccak/` | §8.1.2 | Canonical Keccak benchmark CSVs (ref and opt) |
| `results/simulation/benchmarks/components/decode/` | §8.1.3 | Canonical decode benchmark CSVs (ref, rvv, zvbc) |
| `results/simulation/benchmarks/components/poly_mul/` | §8.1.1 | Canonical polymul benchmark CSVs (all variants) |
| `results/simulation/profiling/histograms/` | §5.3, §9.2 | PC histogram data for instruction mix analysis (scalar/vector) |
| `results/simulation/profiling/traces/` | §5.3 | Spike execution traces for hotspot identification (scalar/vector) |
| `results/simulation/profiling/cpi/reports/` | §5.3, §9.1 | CPI model reports per security level |
| `results/simulation/profiling/cpi/` | §5.3 | CPI model inputs (scalar + vector) |
| `results/simulation/profiling/cache/` | §5.3 | Cache simulation data (scalar/vector) |
| `results/simulation/profiling/mem_access/` | §5.3 | Memory access pattern profiles (scalar/vector) |
| `results/hardware/benchmarks/kem/hw_kem_scalar-ref_latest.csv` | §8.5.2 | Hardware KEM cycles — scalar reference (canonical pointer) |
| `results/hardware/benchmarks/kem/hw_kem_scalar-zb_latest.csv` | §8.5.2 | Hardware KEM cycles — Tier 1 karatsuba_zbc (canonical pointer) |
| `results/hardware/benchmarks/kem/hw_kem_vector-ref_latest.csv` | §8.5.2 | Hardware KEM cycles — vector reference (canonical pointer) |
| `results/hardware/benchmarks/kem/hw_kem_vector-rvv_latest.csv` | §8.5.2 | Hardware KEM cycles — Tier 2 karatsuba_rvv (canonical pointer) |
| `results/hardware/benchmarks/components/poly_mul/hw_mul_scalar-ref_latest.csv` | §8.5.1 | Hardware polymul cycles — scalar reference (canonical pointer) |
| `results/hardware/benchmarks/components/poly_mul/hw_mul_scalar-zb_latest.csv` | §8.5.1 | Hardware polymul cycles — Tier 1 (canonical pointer) |
| `results/hardware/benchmarks/components/poly_mul/hw_mul_vector-ref_latest.csv` | §8.5.1 | Hardware polymul cycles — vector reference (canonical pointer) |
| `results/hardware/benchmarks/components/poly_mul/hw_mul_vector-rvv_latest.csv` | §8.5.1 | Hardware polymul cycles — Tier 2 (canonical pointer) |
| `results/hardware/benchmarks/components/keccak/hw_keccak_ref_latest.csv` | §8.5.1 | Hardware Keccak cycles — reference (canonical pointer) |
| `results/hardware/benchmarks/components/decode/hw_decode_ref_latest.csv` | §8.5.1 | Hardware decode cycles — reference (canonical pointer) |
| `results/hardware/benchmarks/components/decode/hw_decode_rvv_latest.csv` | §8.5.1 | Hardware decode cycles — RVV (canonical pointer) |
| `results/hardware/analysis/vector_rvv_performance_analysis.md` | §8.5.1, §8.5.3 | Detailed vector RVV performance analysis on hardware |
| `results/hardware/sweeps/threshold/` | §8.5.4 | Threshold sweep data for hardware validation |
| `results/reports/summaries/speedups_simulation.md` | §8.2, §10.1 | Simulation speedup summary (instret-based) |
| `results/reports/summaries/speedups_hardware.md` | §8.5.2, §10.1 | Hardware speedup summary (cycles-based) |
| `results/reports/summaries/speedups_cross_platform.md` | §9.1 | Cross-platform comparison summary |
| `results/reports/summaries/rvv_opt_summary.md` | §6.3, §8.2 | RVV optimization summary report |
| `results/reports/summaries/compiler_analysis.md` | §9.5 | Compiler comparison analysis report |
| `results/reports/scoreboards/` | §6.2.6 | Historical scoreboard tracking for polymul experiments |
| `results/reports/figures/pie_scalar-ref.pdf` | §2.2.3, §5.7 | Baseline scalar pie chart |
| `results/reports/figures/pie_vector-zvk.pdf` | §6.5, §7.5 | Final Tier 3 pie chart |
| `results/reports/figures/pie_three_stage_comparison.pdf` | §7.5 | Three-stage Amdahl's law visualization |
| `results/reports/figures/pie_combined.pdf` | §2.2.3 | Combined pie chart comparison |
| `results/reports/figures/bar_kem_speedups.pdf` | §8.2 | KEM speedup bar chart |
| `results/reports/figures/bar_micro_benchmarks.pdf` | §8.1 | Micro-benchmark comparison bar chart |
| `results/reports/figures/bar_stacked_breakdown.pdf` | §8.2 | Stacked breakdown bar chart |
| `results/reports/figures/bar_compiler_comparison.pdf` | §9.5 | Compiler comparison bar chart |
| `results/reports/figures/line_security_scaling.pdf` | §8.3 | Security level scaling line chart |
| `results/reports/figures/line_vlen_sweep.pdf` | §8.4.2 | VLEN sweep line chart |
| `results/reports/figures/heatmap_speedups.pdf` | §8.3 | Speedup heatmap |
| `results/reports/figures/waterfall_ablation.pdf` | §9.2.1 | ISA ablation waterfall chart |
| `results/reports/figures/scatter_code_size.pdf` | §9.3 | Code size vs performance scatter plot |
| `results/reports/figures/speedup_cross_platform.pdf` | §9.1 | Cross-platform speedup comparison |
| `results/reports/figures/hw_spike_vs_hw_bars.pdf` | §8.5.1 | Spike vs HW bars comparison |
| `results/reports/figures/hw_spike_hw_scatter.pdf` | §8.5.4 | Spike prediction vs HW reality scatter |
| `results/reports/figures/hw_ipc_chart.pdf` | §8.5.3 | IPC analysis chart |
| `results/reports/figures/hw_wallclock_bars.pdf` | §8.5.2 | Wall-clock time bar chart |
| `results/ct_verify/valgrind_ct/` | §9.6.3 | Ctgrind XML results for various variants |
| `results/ct_verify/dudect/` | §9.6.4, §9.6.7 | Dudect timing analysis results |
| `docs/proposals/RESULTS/keccak_unrolling_results.md` | §7.1, §7.1.4 | Keccak unrolling experiment results |
| `docs/proposals/RESULTS/load64_intrinsic_results.md` | §7.1.4 | load64 intrinsic optimization results |
| `docs/proposals/RESULTS/reed_solomon_zvbc_results.md` | §7.2.3 | RS Zvbc optimization results |
| `docs/proposals/RESULTS/zbb_ablation_study.md` | §9.2.1 | Zbb ablation quantitative data |
| `docs/figures/cycle_counts.pdf` | §8 | Pre-rendered cycle-count figure for thesis import |
| `docs/figures/instruction_mix.pdf` | §5.3, §9.2 | Pre-rendered instruction-mix figure |
| `docs/figures/latest_dashboard.png` | §8 | Latest combined dashboard snapshot |

---
