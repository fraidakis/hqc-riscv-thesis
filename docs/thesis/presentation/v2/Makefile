# Makefile — HQC-on-RISC-V defense deck (LuaLaTeX + metropolis)
LATEXMK = latexmk
FLAGS   = -lualatex -shell-escape -interaction=nonstopmode -halt-on-error
MAIN    = main

.PHONY: all build handout watch clean distclean

all: build

## build: compile the presentation -> build/main.pdf
build:
	$(LATEXMK) $(FLAGS) $(MAIN).tex

## handout: flatten overlays -> build/handout.pdf (one slide per \pause group)
handout:
	$(LATEXMK) $(FLAGS) handout.tex

## watch: continuous preview rebuild
watch:
	$(LATEXMK) $(FLAGS) -pvc $(MAIN).tex

## clean: remove aux files
clean:
	$(LATEXMK) -c

## distclean: remove everything incl. the PDF and build/
distclean:
	$(LATEXMK) -C
	$(RM) -r build
