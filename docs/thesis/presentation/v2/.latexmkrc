# latexmk configuration for the LuaLaTeX beamer deck
$pdf_mode = 4;            # 4 = lualatex
$out_dir  = 'build';
$lualatex = 'lualatex -shell-escape -interaction=nonstopmode -halt-on-error %O %S';
$bibtex_use = 2;          # run biber for biblatex when needed
$clean_ext = 'nav snm vrb run.xml bbl bcf';
