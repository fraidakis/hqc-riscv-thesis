# =============================================================================
# Defense deck — self-contained build.
# Engine (LuaLaTeX), -shell-escape and build/ are pinned in .latexmkrc.
# =============================================================================
.PHONY: presentation clean

presentation:
	latexmk presentation.tex
	@echo "Presentation PDF: build/presentation.pdf"

clean:
	-latexmk -C presentation.tex
	rm -rf build
