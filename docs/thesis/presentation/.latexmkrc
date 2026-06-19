# .latexmkrc — LuaLaTeX + biber + TikZ externalization for the defense deck
$pdf_mode = 4;            # 4 = lualatex
$lualatex = 'lualatex --shell-escape --interaction=nonstopmode --synctex=1 %O %S';
$biber = 'biber %O %S';
$bibtex_use = 2;

# TikZ externalize writes into build/ext/ -> make sure it exists / is cleaned
$out_dir = '.';
$clean_ext = 'bbl run.xml synctex.gz nav snm vrb';

# externalized figures live under build/ext (see preamble \tikzexternalize)
system('mkdir -p build/ext');
