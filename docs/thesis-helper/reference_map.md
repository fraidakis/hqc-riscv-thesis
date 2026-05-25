# Reference Map: Sections → Source Files

Αυτό το αρχείο αντιστοιχίζει κάθε ενότητα/υπο-ενότητα του `sections.md` στα αρχεία
που πρέπει να διαβαστούν κατά τη συγγραφή. Χρησιμοποιείται ως context για τον AI agent.

Σύμβαση: `→` σημαίνει "ανέτρεξε σε αυτό το αρχείο/ενότητα για λεπτομέρειες".

---

## Κεφάλαιο 1: Εισαγωγή

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §1.1 | `notes/PQC-intro.txt` | "Harvest/Store now - Decrypt later", "CNSA 2.0 mandate", αλγόριθμος Shor |
| §1.2 | `notes/PQC-Algorithms.txt` | "Επιλέχθηκε για standardization 11/03/2025", "HQC vs SPHINCS+", NIST rounds overview |
| §1.3.1 | `notes/PQC-intro.txt` | "SIKE horror story", "Problem 1 - Crypto Agility is Destroyed", "Problem 2 - SIKE" |
| §1.3.2 | `notes/PQC-intro.txt`, `notes/notes_mine.md` | "RISC-V's Own Philosophy Supports Software", "Zbb, V, Zvbb part of the RVA23 standard" |
| §1.4 | `notes/PQC-Algorithms.txt`, `notes/notes_detailed.md` | "Compatibility. Previous works focused on older HQC versions", Discovery 3 speedup table |
| §1.5 | — | Δομή εργασίας, εσωτερική αναφορά |

---

## Κεφάλαιο 2: Θεωρητικό Υπόβαθρο

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §2.1 | `notes/PQC-Algorithms.txt`, `writing/reference_explanation.md` | "HQC Intro", "Algorithm Breakdown - Steps", "Error Code Techniques", security levels |
| §2.2 | `writing/reference_explanation.md` | "Phase 6: The Core Scheme", KEM flow (keygen/encaps/decaps) |
| §2.3 | `optimizations/multiplication.md` §1 | "Baseline Architecture and Starting Point", GF(2)[x] ring, Karatsuba theory |
| §2.4 | `optimizations/keccak.md` §1 | "Baseline Architecture", Sponge Construction, Keccak-f[1600] round structure |
| §2.5 | `optimizations/decode.md` §1 | "Baseline Architecture", RM(1,7), RS over GF(2^8), concatenated code pipeline |

---

## Κεφάλαιο 3: Αρχιτεκτονική RISC-V

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §3.1 | — | RISC history (external literature), modular ISA philosophy |
| §3.2 | `optimizations/multiplication.md` §3.2.1 | "Native clmul/clmulh Injection", Zbc description |
| §3.3 | `writing/VLEN_scaling.md` | VLEN, LMUL, VLA concepts |
| §3.4 | `notes/notes_mine.md` | "Architectural Comparison: SHA-2 vs SHA-3", ARM EOR3/RAX1 vs RISC-V Zvbb |
| §3.5 | `notes/PQC-Algorithms.txt`, `notes/notes_detailed.md` §5.3.1–5.3.2 | "Compatibility. Previous works...", AVX2/NEON context, FAFFT RISC-V paper |

---

## Κεφάλαιο 4: Πειραματική Υποδομή

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §4.1 | `docs/METHODOLOGY.md` §11, §13.1, §13.3 | Spike ISS semantics, QEMU role, dual-simulator strategy |
| §4.2 | `writing/compiler-comparison.md` | Entire document: GCC vs Clang, toolchain files, `-march` flags, LTO/PGO |
| §4.3 | `docs/METHODOLOGY.md` §11, §12 | "Primary metric: retired instruction count", CPI dilemma, instruction mix |
| §4.4 | `docs/ARCHITECTURE.md` | KAT flow, `run_kat.sh`, 300-vector validation |
| §4.5 | `docs/ARCHITECTURE.md` | sim/common/ files: startup.S, link.ld, mem.c, bench.c |

---

## Κεφάλαιο 5: Σχεδιασμός και Βελτιστοποίηση

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §5 intro (pie chart, Amdahl) | `notes/notes_detailed.md` Discovery 6 | "polynomial multiplication was originally about 96% of decaps", "whole-system speedup asymptotically approaches 25x-28x" |
| §5.1 | `notes/notes_detailed.md` Discovery 3 | "3-Tier Hardware Profile", Tier 1/2/3 table, "modular ISA philosophy" |
| §5.2 (intro: γιατί όχι FAFFT) | `notes/notes_detailed.md` §5.3.2.Β, `writing/mult_alternatives.md` | "FAFFT Paradox", "4–5× regression", "TC3 depth-2 was empirically validated as slower" |
| §5.2.1 | `optimizations/multiplication.md` §3 (Track A) | Word-granularity clmul, memset-free discipline |
| §5.2.2 | `optimizations/multiplication.md` §4 (Track B) | RVV combine loops, "leaf is the binding constraint" |
| §5.2.3 | `optimizations/multiplication.md` §5 (Track C) | vclmul_leaf, splat-and-sweep, assign-first-write |
| §5.2.4 | `optimizations/multiplication.md` §3.1.3, `notes/notes_detailed.md` Discovery 1 | AP-2 Generator, "ABI obligations are structural", "static code generation beats compiler" |
| §5.3.1 | `optimizations/keccak.md` §3 | "Track A/B — Shared Baseline: Reference Keccak with Compiler Zbb", rori auto-lowering |
| §5.3.2 | `optimizations/keccak.md` §7.1, `notes/notes_detailed.md` Discovery 2 | "RVV Paradox", "scalar fallback trap", "ISA completeness > ISA width" |
| §5.3.3 | `optimizations/keccak.md` §4.3–4.7, §5 | SoA redesign (C3), 2-way batching (C8), `notes/notes_mine.md` "4-Way Bottleneck" |
| §5.3.4 | `optimizations/keccak.md` §4.6 | "RVV I/O Fast Paths", keccak_xor_bytes, load64_le |
| §5.4.1 | `optimizations/decode.md` §3 | Track A scalar RM+RS path, Zbc gf_mul injection |
| §5.4.2 | `optimizations/decode.md` §4 | Phase 3a RM vectorization, Axis A syndrome vectorization |
| §5.4.3 | `optimizations/decode.md` §5 | RS Axis B (BM), Axis D (Forney), Zvbc lane-packing |
| §5.4.4 | `optimizations/decode.md` §5 "RS Axis C" | FFT root finding (pending), engineering complexity |
| §5.5 | `writing/VLEN_scaling.md` | Entire document: benchmark tables, scaling model, failed approaches, conclusions |
| §5.5.1 | `writing/VLEN_scaling.md` "Instruction Budget Breakdown" | 31% vclmul_leaf, 23% Keccak, 27% recomposition |
| §5.5.2 | `writing/VLEN_scaling.md` "Failed approach" | "threshold = VLMAX → instret INCREASED by 4–11%" |
| §5.5.3 | `writing/VLEN_scaling.md` "What Scales with VLEN" | rvv_copy_u64, rvv_xor3_u64, strip-mined VLA loops |
| §5.5.4 | `writing/VLEN_scaling.md` "Benchmark Results" | Tables per security level, VLEN=512 sweet spot |
| §5.5.5 | `writing/VLEN_scaling.md` "What Does NOT Scale", `notes/notes_detailed.md` Discovery 7 | "Bigger VLEN ≠ fewer instructions", fixed-width coding |
| §5.6 | `optimizations/miscellaneous.md` | Entire document: vect_add, vect_write_support, memcpy/memset, gf_reduce |
| §5.6.1 | `optimizations/miscellaneous.md` §2.1 | O(weight) direct-scatter, 277× reduction |
| §5.6.2 | `optimizations/miscellaneous.md` §2.2 | RVV vect_add, LMUL=8, 20× reduction |
| §5.6.3 | `optimizations/miscellaneous.md` §2.3, §3 | support_contains_value, gf_reduce inlining |
| §5.6.4 | `optimizations/miscellaneous.md` §4 | RVV memcpy/memset, 246× iteration reduction |

---

## Κεφάλαιο 6: Αποτελέσματα

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §6.1 | `analysis/benchmarks/isolated/` CSVs | vect_mul, keccak, decode isolated benchmarks |
| §6.2 | `analysis/benchmarks/` end-to-end CSVs | KEM suite (keypair, encaps, decaps) per level |
| §6.3 | `notes/notes_detailed.md` §5.3.1–5.3.2 | Cross-architecture table (AVX-512, Zen4, M4, FAFFT, ours), instret vs cycles methodology |
| §6.4 | — | HQC-128/192/256 scaling analysis |
| §6.5.1 | `cmake/options.cmake` | HQC_SCALAR_MARCH, ablation ISA variable |
| §6.5.2 | `notes/notes_detailed.md` Discovery 5 | "Zbb alone yields 37% Keccak reduction via native rori" |
| §6.5.3 | `notes/notes_detailed.md` Discovery 4 | "ref_vector vs ref_full_crypto — negligible difference", compiler ceiling |
| §6.5.4 | — | Zvkned PRNG in test-harness only |
| §6.6 | `writing/compiler-comparison.md` | GCC vs Clang codegen analysis, dual-compiler workflow |
| §6.7 | `docs/ARCHITECTURE.md`, `docs/RUNBOOK.md` | Repository structure, script-to-table mapping |

---

## Κεφάλαιο 7: Συμπεράσματα

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| §7.1 | `notes/notes_detailed.md` Discovery 3 | Summary speedup table (x91, x118, x255 vect_mul; x28, x41, x63 decaps) |
| §7.2 | `docs/METHODOLOGY.md` §11, §12 | "We do NOT claim wall-clock time on real hardware", Spike limitations |
| §7.3 | `writing/zkt_ct.md`, `notes/notes_mine.md` "Future of RISC-V & SHA-3" | CT analysis (Zkt model), real hardware, FPGA, PQC Task Group Keccak instructions |

---

## Παράρτημα Α

| Ενότητα | Αρχεία Αναφοράς | Key Phrases / Anchors |
|---------|-----------------|----------------------|
| Α.1 | `toolchain/build-gcc-sysroot.sh`, `toolchain/build-llvm.sh`, `writing/compiler-comparison.md` §3 | Build steps, unified riscv-gnu-toolchain |
| Α.2 | `toolchain/build-spike.sh`, `docs/METHODOLOGY.md` §13.1 | Spike build, --isa flags |

---

## Supplementary: Writing Drafts (pre-written text)

| Αρχείο | Σχετικές Ενότητες | Περιεχόμενο |
|--------|-------------------|-------------|
| `writing/optimizations_overview.md` | §5 overview, §6, presentation slides | Marp slides — condensed narrative of all optimizations with final numbers |
| `writing/reference_explanation.md` | §2.1–2.5 | Detailed vendor code walkthrough, HQC flow, module interactions |
| `writing/mult_alternatives.md` | §5.2 intro, §3.5 | TC3/TC4/FAFFT/sparse analysis, measured regressions, "why Karatsuba wins" |
| `writing/VLEN_scaling.md` | §5.5 | Complete VLEN analysis with tables, ready for thesis text |
| `writing/compiler-comparison.md` | §4.2, §6.6, Appendix A | Full toolchain analysis, dual-compiler workflow, per-phase flags |
| `writing/zkt_ct.md` | §7.3 (μελλοντικό) | Zkt constant-time formal model, engineering diagnostics — deferred to future work |
