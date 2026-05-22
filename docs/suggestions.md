# Suggestions: Agent Strategy for Cataloguing All Optimizations

> Written: 2026-05-22  
> Purpose: recommend an approach for having an agent systematically find,
> describe, and rank every optimization applied on top of the HQC reference
> implementation, ordered by measured speedup impact.

---

## 1. Recommended Strategy: Three-Layer Approach

No single source gives the full picture on its own.  A mix of all three layers
is the most reliable approach.

### Layer 1 — `experiments/manifest.yaml` (ground truth inventory)

This is the single most authoritative source.  Every experiment that was
promoted to stable has:

- Measured `instret` deltas at all three security levels
- A `verdict` field explaining what worked and why
- A `promoted_to` field linking the canonical stable file
- A `status` field to distinguish winners from losers / archived runs

**Use it to:** enumerate all promoted optimizations and extract their measured
`delta_percent` as the primary ranking signal.

### Layer 2 — Component-level source diffs against `vendor/hqc-ref/`

After establishing the inventory from the manifest, run targeted diffs for
each component.  This gives the agent concrete code-level evidence of what
changed and how.

| Optimization domain | Our stable source | Reference source |
|---|---|---|
| GF(2)[x] poly-mul | `src/mult/` | `vendor/hqc-ref/src/ref/gf2x.c` |
| Keccak permutation | `src/keccak/fips202_zvbb_x2.c` | `vendor/hqc-ref/lib/fips202/fips202.c` |
| Reed-Solomon decode | `src/decode/reed_solomon_rvv.c` / `_zvbc.c` | `vendor/hqc-ref/src/ref/reed_solomon.c` |
| Reed-Muller decode | `src/decode/reed_muller_rvv.c` | `vendor/hqc-ref/src/ref/reed_muller.c` |
| Vector support | `src/vector.c` | `vendor/hqc-ref/src/ref/vector.c` |
| Memory primitives | `sim/common/mem.c` | C stdlib `memset`/`memcpy` |

Direct diffs here catch micro-optimizations (unrolling, threshold tuning,
instruction selection) that may not have their own experiment entry.

### Layer 3 — `docs/optimizations/roadmap_*.md` (narrative context)

The roadmaps explain *why* each approach was chosen, the design space explored,
and which alternatives were discarded.  They are essential for writing the
thesis narrative but are **not** a substitute for measured deltas.

Use them to:
- Understand the algorithmic intent behind each diff
- Cross-check the ranked list for completeness
- Identify optimizations that were implemented across multiple experiment
  iterations (e.g. the karatsuba threshold sweep in `roadmap_karatsuba_v2.md`)

---

## 2. Recommended Split by Domain

Splitting the cataloguing task by domain avoids context overload and maps
directly to thesis chapters.

| Domain | Files to diff | Experiments to scan | Roadmap |
|---|---|---|---|   - Read `docs/optimizations/roadmap_*.md` (specifically referencing polynomial multiplication) to extract the "why" (design space, discarded alternatives).

| **Poly-mul** | `src/mult/` vs `vendor/.../gf2x.c` | `EXP-karatsuba-*`, `EXP-zvbc-*`, `EXP-toomcook3-*`, all `archive_karatsuba_*` | `roadmap_karatsuba_{v1,v2,zvbc_v1,zvbc_v2}.md` |
| **Keccak** | `src/keccak/` vs `vendor/.../fips202.c` | `EXP-keccak-*` | `roadmap_keccak.md` |
| **Decode** | `src/decode/` vs `vendor/.../reed_{muller,solomon}.c` | `EXP-decode-rs-*` | `roadmap_decode.md` |
| **Support** | `src/vector.c`, `sim/common/mem.c` vs ref equivalents | `EXP-target2-*`, `EXP-mem-fastpath` | `roadmap_karatsuba_v1.md` (Target 1 & 2) |

---

## 3. What the Agent Should NOT Rely on Alone

- **Experiments alone:** archived and deprecated entries outnumber the winners;
  the agent will over-count unless it filters on `status: promoted_to_stable`.
- **Source diffs alone:** some wins came from algorithm replacement (e.g. scalar
  schoolbook → Karatsuba), which a diff shows only as a full file substitution
  with no quantitative context.
- **Roadmaps alone:** they describe intent and intermediate states, not the
  final measured outcome; numbers in roadmaps may be from intermediate commits.

---

## 4. Ranked Optimization List (most → least impactful)

Rankings are based on measured `instret` deltas from `experiments/manifest.yaml`
and `docs/optimizations/RESULTS/`.  The primary metric is percentage reduction
in total KEM-128 instret (keypair + encaps + decaps combined) unless otherwise
noted.
