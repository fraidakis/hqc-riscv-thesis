# =============================================================================
# Makefile — HQC-on-RISC-V defense deck
# Requires: LuaLaTeX, biber, and --shell-escape (TikZ externalization).
# =============================================================================
MAIN := main
LATEXMK := latexmk

.PHONY: all handout watch view clean veryclean help
.DEFAULT_GOAL := all

# Engine, --shell-escape and $out_dir=build are configured in .latexmkrc, so the
# targets stay thin. Output goes to build/$(MAIN).pdf.
all:
	$(LATEXMK) $(MAIN).tex

# Handout mode: flattens overlays (\pause) into single static slides.
handout:
	$(LATEXMK) -usepretex='\PassOptionsToClass{handout}{beamer}' \
	  -jobname=$(MAIN)-handout $(MAIN).tex

# Continuous preview: rebuild on every save while authoring (Ctrl-C to stop).
watch:
	$(LATEXMK) -pvc $(MAIN).tex

# Open the compiled PDF (now under build/).
view:
	@xdg-open build/$(MAIN).pdf 2>/dev/null || open build/$(MAIN).pdf 2>/dev/null \
	  || echo "Open build/$(MAIN).pdf manually."

# Remove intermediates (keeps build/$(MAIN).pdf); also drop externalized figures
# and any stray auxlock the TikZ externalizer leaves in the source dir.
clean:
	$(LATEXMK) -c
	rm -rf build/ext
	rm -f *.auxlock

# Remove the entire build/ tree, including the compiled PDF.
veryclean:
	$(LATEXMK) -C
	rm -rf build
	rm -f *.auxlock

# List available targets.
help:
	@echo "Targets: all (default), handout, watch, view, clean, veryclean"
