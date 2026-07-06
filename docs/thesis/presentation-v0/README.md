# HQC on RISC-V — Defense Deck

LuaLaTeX + **moloch** Beamer deck for the 15′ thesis defense.

## Quick build

```bash
cd docs/thesis/presentation
make          # -> build/main.pdf
make present  # open in pdfpc (presenter mode + notes)
make handout  # -> build/main-handout.pdf (no overlays)
```

## Requirements

| Package | Install |
|---------|--------|
| LuaLaTeX (TeX Live 2023+) | `tlmgr install lualatex-math` |
| **moloch** Beamer theme | `tlmgr install moloch` |
| Noto Sans fonts | system: `apt install fonts-noto` / `brew install font-noto-sans` |
| Latin Modern Math | bundled with TeX Live |
| pdfpc (optional) | `apt install pdfpc` / `brew install pdfpc` |
| pdfpcnotes.sty (optional) | `tlmgr install pdfpcnotes` — if missing, the deck auto-falls-back to native beamer notes (build still succeeds, just no presenter timer/next-slide preview) |

> **Why moloch and not metropolis?**
> metropolis has a known pgfplots+TikZ overlay interaction bug that breaks
> the externalized figures reused from the thesis. moloch is a maintained
> fork and a drop-in replacement (`\usetheme{moloch}`).

## File structure

```
presentation/
├── main.tex              # root document
├── preamble.tex          # theme, fonts, packages, macros
├── sections/
│   ├── 01-motivation.tex # S1-S3  Κίνητρο & Στόχος
│   ├── 02-background.tex # S4-S5  Υπόβαθρο
│   ├── 03-approach.tex   # S6-S8  Προσέγγιση (polymul tiers)
│   ├── 04-kernels.tex    # S9     Keccak & Decode
│   ├── 05-results.tex    # S10-S11 Αποτελέσματα
│   ├── 06-hardware.tex   # S12    Hardware validation
│   ├── 07-crossarch.tex  # S13    Cross-architecture
│   └── 08-conclusion.tex # S14    Συμπεράσματα
└── backup.tex            # Q&A backup (\appendix)
```

## Reused figures (\input from thesis)

| File | Frame |
|------|-------|
| `figures/ch1-introduction/timeline-pqc.tex` | S1 |
| `figures/ch1-introduction/acceleration-spectrum.tex` | S2 |
| `figures/ch6-polymul/polymul-tiers.tex` | S3 |
| `figures/ch2-background/hqc-kem-sequence.tex` | S4 |
| `figures/ch2-background/pie-scalar-ref.tex` | S5 |
| `figures/ch6-polymul/pie-vector-zvk.tex` | S11 |
| `figures/ch3-riscv/clmul-semantics.tex` | S6 |
| `figures/ch8-results/scaling-security-line.tex` | B3 |
| `figures/ch8-results/microbench-bar-chart.tex` | B4 |

`figures/ch9-discussion/cross-architecture-table.tex` uses `\cref`/`\cite`
→ recreated as a bib-less slide table in `sections/07-crossarch.tex`.

## TikZ externalization (optional)

New slide TikZ figures carry `\tikzsetnextfilename{pres-...}` names.
The `external` library is loaded (so `\tikzsetnextfilename` is always defined).
To enable full externalization (deterministic figure cache):

```tex
% in preamble.tex, uncomment:
\tikzexternalize[prefix=figures-cache/]{main}
```

Requires `-shell-escape` — already set in `.latexmkrc`.

## Slide map

| Frame | Section | Title (condensed) | Time |
|-------|---------|-------------------|------|
| S1 | Κίνητρο | Quantum threat / NIST HQC | 1′ |
| S2 | Κίνητρο | RISC-V ISA ext opportunity | 1′ |
| S3 | Κίνητρο | Contribution statement | 1′ |
| S4 | Υπόβαθρο | HQC KEM structure | 1′ |
| S5 | Υπόβαθρο | 96.6% instret in polymul | 1′ |
| S6 | Προσέγγιση | Tier1 scalar Zbc ×101 | 1′ |
| S7 | Προσέγγιση | Tier2 Base-RVV ×124 | 45″ |
| S8 | Προσέγγιση | Tier3 Zvbc ×264 | 1′ |
| S9 | Προσέγγιση | Keccak ×1.37 + Decode ×14.3 | 45″ |
| S10 | Αποτελέσματα | Full KEM ×76/×164 | 1′ |
| S11 | Αποτελέσματα | Profile redistribution | 1′ |
| S12 | Αποτελέσματα | Hardware ×46/×78, IPC 1.49 | 45″ |
| S13 | Αποτελέσματα | Cross-architecture table | 45″ |
| S14 | Συμπεράσματα | Conclusion + future | 1.5′ |
| **Total** | | | **~15′** |

## Known placeholders (confirm before printing)

- `main.tex`: defense date = "Ιούλιος 2026" (confirm exact date)
- `main.tex`: 2nd and 3rd committee members (commented out; add in `\institute`)
- `sections/03-approach.tex`: HQC-192 Tier2/3 polymul speedup annotated as approx; recheck with final benchmarks
