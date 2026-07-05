# .latexmkrc — LuaLaTeX + biber + TikZ externalization for the defense deck
$pdf_mode = 4;            # 4 = lualatex
$lualatex = 'lualatex --shell-escape --halt-on-error --file-line-error --interaction=nonstopmode --synctex=1 %O %S';
$biber = 'biber %O %S';
$bibtex_use = 2;
$max_repeat = 7;         # extra passes for cleveref + totalframenumber convergence

$clean_ext = 'bbl run.xml synctex.gz nav snm vrb auxlock';

# make sure the externalization target dir exists before the first run
system('mkdir -p build/ext');
