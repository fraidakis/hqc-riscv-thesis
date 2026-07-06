# =============================================================================
# Makefile — HQC on RISC-V defense deck (LuaLaTeX + moloch)
# Requires: latexmk, LuaLaTeX, moloch theme (tlmgr install moloch)
# =============================================================================

MAIN    := main
BUILD   := build
LATEXMK := latexmk
FLAGS   := -lualatex -shell-escape -interaction=nonstopmode -halt-on-error

.PHONY: all build handout watch present clean distclean

# Default: build presentation PDF (ALWAYS pass an explicit target file --
# a bare `latexmk` call falls back to globbing *.tex and will also try to
# compile preamble.tex/backup.tex as standalone root documents and fail).
all: build

build:
	$(LATEXMK) $(FLAGS) $(MAIN).tex

# Handout version (no overlays, for printing / Q&A fallback)
handout:
	$(LATEXMK) $(FLAGS) -jobname=$(MAIN)-handout \
	  -usepretex="\PassOptionsToClass{handout}{beamer}" $(MAIN).tex

# Open in pdfpc (presenter mode with notes + timer); -d = duration in minutes
present: build
	pdfpc -d 15 $(BUILD)/$(MAIN).pdf

# Continuous rebuild
watch:
	$(LATEXMK) $(FLAGS) -pvc $(MAIN).tex

# Remove latexmk artifacts
clean:
	$(LATEXMK) -c
	rm -f *.nav *.snm *.vrb

# Remove everything including generated PDFs
distclean:
	$(LATEXMK) -C
	rm -rf $(BUILD) figures-cache
	rm -f *.nav *.snm *.vrb $(MAIN)-handout.pdf
