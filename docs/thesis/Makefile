# =============================================================================
# Thesis Build Targets
# =============================================================================
.PHONY: pdf clean watch count lint open

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

# Clean all build artifacts
clean:
	latexmk -C
	rm -rf $(BUILD_DIR)
	rm -rf _minted-main

# Clean and rebuild
rebuild: clean pdf

# Export final PDF with proper name for submission
export: pdf
	@cp build/main.pdf "Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
	@echo "Exported: Fraidakis_Diploma_Thesis_HQC_RISCV.pdf"
