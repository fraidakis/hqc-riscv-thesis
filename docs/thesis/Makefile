# =============================================================================
# Thesis Build Targets
# =============================================================================
.PHONY: pdf clean watch count lint open spell todos rebuild

BUILD_DIR = build

# Default target — full build via latexmk
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

# Clean all build artifacts (including biber state files that can get corrupted)
# The leading - on latexmk -C tells make to ignore its exit code, so that
# rm -rf build always runs even when the build state is broken (which is
# exactly the situation where a clean is needed most).
clean:
	-latexmk -C
	rm -rf $(BUILD_DIR)
	rm -rf _minted-main

# Clean and rebuild
rebuild: clean pdf

# Export final PDF with proper name for submission
export: pdf
	@cp build/main.pdf "Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
	@echo "Exported: Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
