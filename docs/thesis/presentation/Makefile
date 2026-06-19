# =============================================================================
# Makefile — HQC-on-RISC-V defense deck
# Requires: LuaLaTeX, biber, and --shell-escape (TikZ externalization).
# =============================================================================
MAIN := main
LATEXMK := latexmk

.PHONY: all handout clean veryclean

all:
	$(LATEXMK) -lualatex -shell-escape $(MAIN).tex

# Handout mode: flattens overlays (\pause) into single static slides.
handout:
	$(LATEXMK) -lualatex -shell-escape \
	  -usepretex='\PassOptionsToClass{handout}{beamer}' \
	  -jobname=$(MAIN)-handout $(MAIN).tex

clean:
	$(LATEXMK) -c
	rm -rf build/ext

veryclean:
	$(LATEXMK) -C
	rm -rf build
