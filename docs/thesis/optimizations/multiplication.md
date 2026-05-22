# Improved Agent Prompt: Polynomial Multiplication Optimization Documentation

> This file contains the revised `@workspace` prompt to be submitted to the
> agent.  It corrects all path errors and structural gaps present in the
> original draft, and adds precise source-of-truth references so the agent
> produces a complete, thesis-quality section on its first run.

---

## Changes From Original Prompt (Summary)

| Issue in original | Fix applied |
|---|---|
| `vendor/hqc-ref/lib/gf2x/` does not exist | Corrected to `vendor/hqc-ref/src/ref/gf2x.c` + `gf2x.h` |
| `experiments/manifest.yaml` not mentioned | Added as **primary metric source** (filter `promoted_to_stable`) |
| Archive experiments ignored | Added `experiments/dropped/archive_karatsuba_*/` for step-by-step progression |
| Only two roadmaps mentioned | Named all four explicitly: v1, v2, zvbc_v1, zvbc_v2 |
| ISA track structure absent | Added Track A (Zbc), B (RVV-only), C (Zvbc) framing |
| AP-2 fixed-tree generator missing | Added `karatsuba_specialized_gen.h` + `karatsuba_zvbc_specialized_gen.h` |
| Legacy poly helpers absent | Removed — `poly_helpers` deleted (unused by production) |
| Hybrid path in stable tree missing | Clarified as archived-only under `experiments/dropped/archive_hybrid_zbc/` |
| No thesis writing style specified | Added academic prose requirement aligned with sections.md §5.2 |
| Ranking criterion vague | Replaced with three-tier ranking keyed to `delta_percent` from manifest |

---

## Revised Prompt

```
@workspace

We are writing Section 5.2 ("Vectorized Polynomial Multiplication Acceleration")
of a RISC-V / HQC thesis. We need a complete, thesis-quality documentation of
every algorithmic and microarchitectural optimization applied to the GF(2)[x]
polynomial multiplication subsystem. Prioritize absolute completeness; do not
omit any optimization even if it appears minor.

─────────────────────────────────────────────────────────────
PART 1 — SOURCES TO READ (all mandatory)
─────────────────────────────────────────────────────────────

1a. REFERENCE BASELINE (what we started from):
    - `vendor/hqc-ref/src/ref/gf2x.c`         ← scalar O(n²) schoolbook multiplier
    - `vendor/hqc-ref/src/ref/gf2x.h`         ← API contract

1b. STABLE OPTIMIZED SOURCE TREE:
    - `src/mult/karatsuba_zbc/gf2x_clmul.c`
      → Track A champion: Zbc-accelerated Karatsuba with fixed-size leaf unrolling
    - `src/mult/karatsuba_zbc/karatsuba_specialized_gen.h`
      → AP-2 fixed-recursion-tree generator (scalar)
    - `src/mult/karatsuba_zvbc/gf2x_vclmul.c`
      → Track C champion: Zvbc vclmul.vv Karatsuba
    - `src/mult/karatsuba_zvbc/karatsuba_zvbc_specialized_gen.h`
      → AP-2 fixed-recursion-tree generator (vector)

1c. EXPERIMENT MANIFEST (primary metric source):
    - `experiments/manifest.yaml`
      → Filter on `status: promoted_to_stable`. For each poly-mul entry read:
        `id`, `promoted_to`, `verdict`, all `metrics.*` blocks.
      → The `delta_percent` field is the authoritative speedup ranking key.
      → Use the experiment directory split: `experiments/promoted/` for
        promoted artifacts and `experiments/dropped/` for deprecated/archived
        artifacts.
      Relevant experiment IDs to locate:
        EXP-karatsuba-zbc-champion
        EXP-karatsuba-zvbc-champion
        EXP-karatsuba-zbc-leaf-fixed
        EXP-karatsuba-v2-a5
        EXP-karatsuba-ablation
        EXP-karatsuba-ap2-specialized
        EXP-karatsuba-leaf-v2-micro
        EXP-zvbc-schoolbook
        EXP-toomcook3-gf2x-zbc
        EXP-target2-bitmanip-scatter
        (also read deprecated/archived entries for discarded-alternative context)

1d. ARCHIVE EXPERIMENTS (step-by-step evolution):
    Read each directory listed below; for every one check for a
    `results.md`, `bench.txt`, or top-level `.c` file:
  - `experiments/dropped/archive_karatsuba_zbc_exp/`
  - `experiments/dropped/archive_karatsuba_zbc_pre_ap2/`
  - `experiments/dropped/archive_karatsuba_zbc_pre_leaffixed/`
  - `experiments/dropped/archive_karatsuba_zbc_pre_nomemset/`
  - `experiments/dropped/archive_karatsuba_zbc_pre_scalarsweep/`
  - `experiments/dropped/archive_karatsuba_zbc_pre_target2/`
  - `experiments/dropped/archive_karatsuba_zbc_pre_target4/`
  - `experiments/dropped/archive_karatsuba_zvbc_pre_ap2/`
  - `experiments/dropped/archive_karatsuba_zvbc_prototypes/`

1e. ROADMAPS (design rationale and discarded alternatives):
    Read all four in full:
    - `docs/optimizations/roadmap_karatsuba_v1.md`   ← Targets T1–T4 (memset, scatter, gf_mul, leaf pipeline)
    - `docs/optimizations/roadmap_karatsuba_v2.md`   ← V2 targets (leaf unrolling, threshold sweep, AP-2)
    - `docs/optimizations/roadmap_karatsuba_zvbc_v1.md`  ← Track C entry: scalar→Zvbc transition, schoolbook baseline
    - `docs/optimizations/roadmap_karatsuba_zvbc_v2.md`  ← Track C v2: final 30% profiling, memset/keccak analysis

1f. MEASURED RESULTS (numbers to fill metric gaps):
    - `docs/optimizations/RESULTS/` — read all files present
    - `docs/optimizations/karatsuba_zvbc_v2_memset_cycle-explanation.md`

─────────────────────────────────────────────────────────────
PART 2 — REQUIRED FRAMING: ISA TRACK STRUCTURE
─────────────────────────────────────────────────────────────

The optimizations were developed across three ISA tracks. Every optimization
must be labelled with its track:

| Track | ISA extensions used | Dominant technique |
|---|---|---|
| Track A | Zbc (clmul/clmulh), Zbb, Zbkb | Scalar Karatsuba + clmul leaves |
| Track B | RVV 1.0 (V extension only, no crypto) | Vector XOR-combine loops |
| Track C | Zvbc (vclmul.vv/vclmulh.vv) + V | Vector carry-less multiply leaves |

Track B was explored but produced only −3.55% over Track A because the leaf
bottleneck remained scalar. Track C eliminated this by replacing scalar
clmul leaves with vclmul.vv, achieving ~−35% over Track B.

Current stable CMake option set for Keccak is:
- `HQC_KECCAK_IMPL=ref`
- `HQC_KECCAK_IMPL=zvbb_x2_soa`
(the former experiment-only selector was removed after promotion).

─────────────────────────────────────────────────────────────
PART 3 — EXTRACTION RULES
─────────────────────────────────────────────────────────────

3a. Document EVERY optimization, including:
    - Algorithm-level changes (schoolbook → Karatsuba, recursion depth, base case)
    - ISA-level changes (scalar clmul → vclmul.vv)
    - Micro-level changes (leaf unrolling, threshold tuning, XOR-combine loop
      unrolling, register allocation, branch elimination)
    - Data-layout changes (SoA vs AoS, in-place accumulation, zero-copy output)
    - Support changes directly enabling poly-mul throughput
      (vect_write_support_to_vector O(n·w)→O(w) scatter; include this even
      though it lives in src/vector.c)
    - Discarded alternatives (Toom-Cook 3-way, FAFFT, RVV-only leaf) with
      the measured or qualitative reason for rejection

3b. Ranking tiers (use measured `delta_percent` from manifest as primary key):
    - **Tier 1 (>10% total KEM reduction or >5× isolated speedup):**
      algorithmic/ISA changes that restructured the computation
    - **Tier 2 (1–10% total KEM reduction):**
      micro-optimizations and threshold tuning on top of Tier 1
    - **Tier 3 (<1% or unquantified):**
      code-quality, layout, and register-management improvements

3c. For metrics: use manifest `delta_percent` if available. For pre-manifest
    archive entries, use numbers from roadmap tables or note as
    "pre-manifest baseline" with the roadmap source. Never invent numbers.

3d. For discarded alternatives, write one concise paragraph per alternative
    citing the specific measured result or qualitative reason.

─────────────────────────────────────────────────────────────
PART 4 — OUTPUT SPECIFICATION
─────────────────────────────────────────────────────────────

Write the output as a self-contained thesis section in English. Style:
academic, precise, third-person. Avoid marketing language. Use past tense
for optimizations that were completed. Use LaTeX-compatible math notation
inline ($...$) for any formulae.

File to create: `docs/thesis/optimizations/multiplication.md`

The file must contain the following top-level sections in order:

## 1. Baseline: Reference Scalar Polynomial Multiplication
   Describe the O(n²) schoolbook algorithm in gf2x.c. Note the HQC-128
   instret at baseline (~18 578 007 vect_mul instret per the roadmap tables).

## 2. Optimization Progression Overview
   One concise table mapping each optimization to its track, tier, and
   cumulative decaps instret after promotion. Use the manifest data.
   Columns: Optimization | Track | Tier | vect_mul instret (L128) | Δ vs prior step

## 3. Tier 1 Optimizations (ranked, most impactful first)
   For each:
   - **Title** (bold, concise)
   - *Baseline behavior:* what the code did before
   - *Optimized behavior:* what was changed, with code structure description
     (no need to copy source verbatim; describe the structure)
   - *Microarchitectural rationale:* why this helps on RVA23-class cores
   - *ISA track:* A / B / C
   - *Measured impact:* table row with instret figures across L128/L192/L256
   - *Source file:* path in src/ and corresponding experiment ID in manifest

## 4. Tier 2 Optimizations (same sub-structure as Tier 1)

## 5. Tier 3 Optimizations (condensed, grouped if appropriate)

## 6. Discarded Alternatives
   One subsection per rejected approach. Include: what was tried, why it
   seemed promising, what the measurement showed, and why it was archived.

## 7. Cumulative Impact Summary
   Final table: Reference → karatsuba_zbc → karatsuba_zvbc champion.
   Rows: HQC-128, HQC-192, HQC-256.
   Columns: Total KEM instret, vect_mul isolated instret, decaps instret.
   Source the numbers from manifest `metrics` blocks and roadmap tables.
```
