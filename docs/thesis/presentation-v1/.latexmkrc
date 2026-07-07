# .latexmkrc — LuaLaTeX + biber + TikZ externalization for the defense deck
$pdf_mode = 4;            # 4 = lualatex
$lualatex = 'lualatex --shell-escape --halt-on-error --file-line-error --interaction=nonstopmode --synctex=1 %O %S';
$biber = 'biber %O %S';
$bibtex_use = 2;
$max_repeat = 7;         # extra passes for cleveref + totalframenumber convergence

$out_dir = 'build';       # isolate all build artifacts under build/
$aux_dir = 'build';       # keep the presentation source tree clean

$clean_ext = 'bbl run.xml synctex.gz nav snm vrb auxlock';

# Externalization is disabled in preamble.tex (figures render inline), so no
# build/ext bootstrap or symlink bridge is needed here.
