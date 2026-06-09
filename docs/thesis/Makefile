# =============================================================================
# Thesis Build Targets
# =============================================================================
.PHONY: pdf clean watch count lint open spell todos rebuild rebuild-full extern-clean draft chapter-only figures-parallel

BUILD_DIR = build
JOBS ?= 6

# Default target — full build via latexmk (figures served from cache)
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
	@chktex -q main.tex chapters/*.tex 2>/dev/null || \
		echo "Install chktex: sudo apt install chktex"

# Spell-check all chapter text (English words via aspell; requires aspell-en)
# For Greek text install: sudo apt install aspell-el
spell:
	@echo "=== Spell-checking English passages (aspell-en) ==="
	@for f in chapters/*.tex frontmatter/*.tex appendices/*.tex; do \
		echo "--- $$f ---"; \
		aspell --lang=en_GB --mode=tex --encoding=utf-8 list < "$$f" 2>/dev/null | sort -u; \
	done || echo "Install aspell: sudo apt install aspell aspell-en aspell-el"

# List all outstanding \\todo{} / \\missingfig{} markers in chapters
todos:
	@grep -rn --color=always '\\todo\|\\missingfig\|\\missingcite\|\\placeholder\|\\itodo\|% TODO' \
		chapters/ frontmatter/ appendices/ 2>/dev/null | grep -v '^Binary' || echo "No TODOs found — thesis is complete!"

# Clean all build artifacts — PRESERVES build/figures-cache/ (figure cache is
# expensive to rebuild; use 'make extern-clean' to wipe it explicitly).
# The leading - on latexmk -C tells make to ignore its exit code, so that
# rm -rf always runs even when the build state is broken.
clean:
	-latexmk -C
	rm -rf $(BUILD_DIR)/chapters $(BUILD_DIR)/frontmatter $(BUILD_DIR)/appendices
	rm -f $(BUILD_DIR)/main.pdf $(BUILD_DIR)/main.aux $(BUILD_DIR)/main.log
	rm -f $(BUILD_DIR)/main.bcf $(BUILD_DIR)/main.bbl $(BUILD_DIR)/main.blg
	rm -f $(BUILD_DIR)/main.toc $(BUILD_DIR)/main.lof $(BUILD_DIR)/main.lot
	rm -f $(BUILD_DIR)/main.lol $(BUILD_DIR)/main.out $(BUILD_DIR)/main.synctex.gz
	rm -f $(BUILD_DIR)/main.fdb_latexmk $(BUILD_DIR)/main.fls $(BUILD_DIR)/main.makefile
	rm -rf _minted-main

# Clean and rebuild (keeps figure cache intact)
rebuild: clean pdf

# Full rebuild — wipes everything including figure cache, then renders in parallel
rebuild-full: extern-clean clean figures-parallel

# Wipe only the TikZ figure cache — forces re-render of all figures next build.
# Use after editing figure .tex files if a stale cached PDF persists.
# The figures-cache symlink is recreated automatically by latexmk on next run.
extern-clean:
	rm -rf build/figures-cache
	rm -f figures-cache
	@echo "Figure cache cleared. Next build will re-render all figures."

# Parallel figure rendering — renders all TikZ figures using $(JOBS) parallel
# workers, then does the final latexmk pass (which reads from cache).
# Use this for a cold build or after 'make extern-clean'. Default: JOBS=12.
# Override: make figures-parallel JOBS=6
#
# How it works:
#   1. A single lualatex pass with mode=list and make writes build/main.makefile
#      listing every figure as a make target (no rendering yet, so it's fast).
#   2. make -j$(JOBS) -f build/main.makefile renders all figures in parallel.
#   3. Normal latexmk run reads every figure from cache — no more subprocesses.
figures-parallel:
	@echo "=== Pass 1: generating figure list (build/main.makefile) ==="
	@mkdir -p build/figures-cache build/chapters build/frontmatter build/appendices
	@ln -sfn build/figures-cache figures-cache 2>/dev/null || true
	-lualatex -shell-escape -interaction=batchmode -output-directory=build \
		'\def\tikzListAndMake{1}\input{main}'
	@test -f build/main.makefile || { echo "ERROR: build/main.makefile was not generated. Check build/main.log"; exit 1; }
	@# The generated makefile uses 'cat main.figlist' (no path prefix), so
	@# symlink it at the thesis root so make -f build/main.makefile can find it.
	@ln -sfn build/main.figlist main.figlist 2>/dev/null || true
	@echo "=== Pass 2: rendering $(JOBS) figures in parallel ==="
	make -j$(JOBS) -f build/main.makefile
	@rm -f main.figlist
	@echo "=== Pass 3: final latexmk build (all figures cached) ==="
	latexmk

# Draft mode — skip PDF output entirely (fastest text-layout check, ~30% faster per pass).
# Figures appear as empty boxes; synctex still works.
draft:
	latexmk -f -e '$$lualatex = q/lualatex -shell-escape -interaction=nonstopmode -draftmode %O %S/'

# Single-chapter mode — compile only one chapter to speed up focused editing.
# Usage: make chapter-only chapter=08-results
chapter ?= 01-introduction
chapter-only:
	latexmk -f -e '$$lualatex = q/lualatex -shell-escape -interaction=nonstopmode -synctex=1 "\\PassOptionsToClass{}{scrbook}\\includeonly{chapters/$(chapter)}" %O %S/'

# Export final PDF with proper name for submission
export: pdf
	@cp build/main.pdf "Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
	@echo "Exported: Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
