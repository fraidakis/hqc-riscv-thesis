# =============================================================================
# Thesis Build Targets
# =============================================================================
.PHONY: pdf clean watch count lint open todos rebuild rebuild-full \
        extern-clean chapter-only figures-parallel

BUILD_DIR = build

# How many parallel lualatex jobs for 'make figures-parallel'.
# Each job loads the full document into memory (~300 MB RAM).
# Default=4 is safe for a 16 GB laptop
JOBS ?= 6

# Default target — full build via latexmk (figures served from cache).
# If a figure's source changed, tikz/external detects the md5 mismatch and
# re-renders ONLY that figure inline — no manual intervention needed.
pdf:
	latexmk

# Continuous build (watches for changes, auto-rebuilds)
watch:
	latexmk -pvc

# Open the built PDF
open: pdf
	@xdg-open $(BUILD_DIR)/main.pdf 2>/dev/null || echo "PDF at $(BUILD_DIR)/main.pdf"

# Word count (chapters + frontmatter)
count:
	@texcount -inc -total chapters/*.tex frontmatter/*.tex appendices/*.tex 2>/dev/null || \
		echo "Install texcount: sudo apt install texcount"

# Lint with chktex
lint:
	@command -v chktex >/dev/null 2>&1 || { echo "Install chktex: sudo apt install chktex"; exit 1; }
	@chktex -q main.tex chapters/*.tex figures/**/*.tex 2>/dev/null; true

# List all outstanding \todo{} / \missingfig{} markers in chapters
todos:
	@grep -rn --color=always '\\todo\|\\missingfig\|\\missingcite\|\\placeholder\|\\itodo\|% TODO' \
		chapters/ frontmatter/ appendices/ 2>/dev/null | grep -v '^Binary' || echo "No TODOs found — thesis is complete!"

# Clean all build artifacts — PRESERVES build/figures-cache/ (the cache is
# expensive to rebuild; use 'make extern-clean' to wipe it explicitly).
# The leading - on latexmk -C tells make to ignore its exit code.
clean:
	-latexmk -C
	rm -rf $(BUILD_DIR)/chapters $(BUILD_DIR)/frontmatter $(BUILD_DIR)/appendices
	rm -rf $(BUILD_DIR)/chapter-preview
	rm -f $(BUILD_DIR)/main.pdf $(BUILD_DIR)/main.aux $(BUILD_DIR)/main.log
	rm -f $(BUILD_DIR)/main.bcf $(BUILD_DIR)/main.bbl $(BUILD_DIR)/main.blg
	rm -f $(BUILD_DIR)/main.toc $(BUILD_DIR)/main.lof $(BUILD_DIR)/main.lot
	rm -f $(BUILD_DIR)/main.lol $(BUILD_DIR)/main.out $(BUILD_DIR)/main.synctex.gz
	rm -f $(BUILD_DIR)/main.fdb_latexmk $(BUILD_DIR)/main.fls $(BUILD_DIR)/main.makefile
	rm -rf _minted-main

# Clean and rebuild (keeps figure cache intact)
rebuild: clean pdf

# Full rebuild — wipes everything including figure cache, then re-renders in parallel
rebuild-full: extern-clean clean figures-parallel

# Wipe only the TikZ figure cache.
# Normally NOT needed: tikz/external auto-detects changed figures via md5.
# Use only if you suspect a corrupt cached PDF (e.g. after a hard reset mid-build).
extern-clean:
	rm -rf build/figures-cache
	rm -f figures-cache
	@echo "Figure cache cleared. Next 'make figures-parallel' will re-render all figures."

# Remove build-time leftovers from the figure cache, keeping only the 3 files
# that are actually needed at runtime (.pdf, .md5, .dpth). Called automatically
# by figures-parallel — not a public target.
#   .log   — lualatex subprocess log (~400 KB each, ~18 MB for 45 figures)
#   .dep   — list-and-make dependency stub (only used during figures-parallel pass 2)
#   .aux   — subplot aux file (not needed after rendering)
#   .bcf / .run.xml — biber state from subprocess (figures don't use bibliography)
_cache-trim:
	@echo "Trimming figure cache (keeping .pdf .md5 .dpth) ..."
	@find build/figures-cache -name '*.log' -o -name '*.dep' \
	    -o -name '*.aux' -o -name '*.bcf' -o -name '*.run.xml' \
	    | xargs rm -f
	@echo "Done. Cache size: $$(du -sh build/figures-cache | cut -f1)"

# Parallel figure rendering — renders all TikZ figures using $(JOBS) parallel
# workers, then does the final latexmk pass (which reads from cache).
#
# When to use:
#   • Cold build (empty cache) — avoids serial one-at-a-time subprocess bottleneck
#   • After 'make extern-clean' — same reason
#
# Normal editing workflow (do NOT use this):
#   Just run 'make pdf'. tikz/external computes an md5 of each figure's source
#   on every run and re-renders only the figures whose source changed. Automatic,
#   no manual step needed.
#
# How it works:
#   Pass 1 — lualatex with mode=list and make: scans the document and writes
#             build/main.makefile listing every figure as a make target. Fast
#             (~60 s), no rendering.
#   Pass 2 — make -j$(JOBS) -f build/main.makefile: spawns $(JOBS) lualatex
#             processes in parallel, each rendering one figure.
#   Pass 3 — normal latexmk: all figures are in cache, so this is a fast run.
#   Trim  — removes .log/.dep/.aux/.bcf/.run.xml leftovers from the cache.
figures-parallel:
	@echo "=== Pass 1: generating figure list (build/main.makefile) ==="
	@mkdir -p build/figures-cache build/chapters build/frontmatter build/appendices
	@ln -sfn build/figures-cache figures-cache 2>/dev/null || true
	-lualatex -shell-escape -interaction=batchmode -output-directory=build \
		'\def\tikzListAndMake{1}\input{main}'
	@test -f build/main.makefile || \
		{ echo "ERROR: build/main.makefile not generated — check build/main.log"; exit 1; }
	@# The generated makefile's ALL_FIGURE_NAMES uses '$(shell cat main.figlist)'.
	@# figlist is in build/ but make runs from thesis root, so symlink it.
	@ln -sfn build/main.figlist main.figlist 2>/dev/null || true
	@echo "=== Pass 2: rendering figures ($(JOBS) parallel jobs) ==="
	make -j$(JOBS) -f build/main.makefile
	@rm -f main.figlist
	@echo "=== Pass 3: final latexmk build (all figures from cache) ==="
	latexmk
	@$(MAKE) _cache-trim

# Single-chapter mode — compile only one chapter to speed up focused editing.
#
# Usage (short form):  make ch=1   make ch=2  ...  make ch=10
# Usage (long form):   make chapter-only chapter=01-introduction
#
# Output goes to build/chapter-preview/main.pdf — never overwrites build/main.pdf.
#
# Figures are LIVE: tikz-external checks md5 for each figure in the chapter.
#   Unchanged figures load instantly from the shared build/figures-cache/.
#   Modified or new figures are re-rendered on-the-fly (~30s per figure).
#   This works because every figure uses \tikzsetnextfilename{stable-name},
#   making cached PDF names deterministic regardless of \includeonly.
#
# The build dir is isolated (build/chapter-preview/) so lualatex never touches
# build/main.aux.  A symlink build/chapter-preview/figures-cache → ../figures-cache
# ensures both the main process and figure subprocesses share one cache.

# Map short chapter numbers (ch=1..10) to full filenames.
ifdef ch
  ifeq ($(ch),1)
    chapter := 01-introduction
  else ifeq ($(ch),2)
    chapter := 02-theoretical-background
  else ifeq ($(ch),3)
    chapter := 03-risc-v-architecture
  else ifeq ($(ch),4)
    chapter := 04-related-work
  else ifeq ($(ch),5)
    chapter := 05-experimental-setup
  else ifeq ($(ch),6)
    chapter := 06-polymul-optimization
  else ifeq ($(ch),7)
    chapter := 07-kernel-optimization
  else ifeq ($(ch),8)
    chapter := 08-results
  else ifeq ($(ch),9)
    chapter := 09-discussion
  else ifeq ($(ch),10)
    chapter := 10-conclusions
  else
    $(error Unknown ch=$(ch). Valid values: 1-10)
  endif
endif

chapter ?= 01-introduction

chapter-only:
	@echo "Compiling chapter: $(chapter)"
	@# Clean stale per-run artifacts but keep the build dir across runs.
	@rm -f  build/chapter-preview/main.aux   build/chapter-preview/main.auxlock \
	        build/chapter-preview/main.bcf   build/chapter-preview/main.run.xml \
	        build/chapter-preview/main.toc   build/chapter-preview/main.lof \
	        build/chapter-preview/main.lot   build/chapter-preview/main.lol \
	        build/chapter-preview/main.out   build/chapter-preview/main.synctex.gz
	@mkdir -p build/chapter-preview/chapters \
	           build/chapter-preview/frontmatter \
	           build/chapter-preview/appendices
	@# Shared figure cache: both the main process (-output-directory=build/chapter-preview)
	@# and the subprocess (-output-directory=build) must resolve figures-cache/ to the
	@# same real directory.  Symlink build/chapter-preview/figures-cache → ../figures-cache
	@# so .md5 writes from the main process land in build/figures-cache/ alongside the PDFs.
	@mkdir -p build/figures-cache
	@ln -sfn ../figures-cache build/chapter-preview/figures-cache 2>/dev/null || true
	@ln -sfn build/figures-cache figures-cache 2>/dev/null || true
	lualatex -shell-escape -interaction=batchmode \
		-output-directory=build/chapter-preview \
		'\PassOptionsToClass{}{scrbook}\def\tikzChapterOnly{1}\includeonly{chapters/$(chapter)}\input{main}'
	@echo "Chapter PDF: build/chapter-preview/main.pdf"

# Export final PDF with proper name for submission
export: pdf
	@cp build/main.pdf "Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
	@echo "Exported: Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
