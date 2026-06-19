# =============================================================================
# latexmk configuration — defense deck (LuaLaTeX)
# =============================================================================
$pdf_mode = 4;                  # 4 = LuaLaTeX
$lualatex = 'lualatex -shell-escape -interaction=nonstopmode -synctex=1 %O %S';
$out_dir  = 'build';

# Beamer-specific aux files cleaned by `latexmk -c/-C`.
push @generated_exts, 'nav', 'snm', 'vrb';
