# =============================================================================
# latexmk configuration — LuaLaTeX
# =============================================================================

$pdf_mode = 4;  # lualatex

# Route all build artifacts to build/ subdirectory
$out_dir = 'build';
$aux_dir = 'build';

# Pre-create needed subdirectories in build/ for \include aux files
use File::Path qw(make_path);
make_path("build/chapters", "build/frontmatter", "build/appendices");

# Shell-escape for minted, synctex for editor integration
$lualatex = 'lualatex -shell-escape -interaction=nonstopmode -synctex=1 %O %S';

# Use biber (not bibtex) — override the bibtex command to call biber
$bibtex = 'biber --input-directory . %O %B';

# Clean extensions
$clean_ext = 'bbl bcf blg run.xml synctex.gz fdb_latexmk fls aux log out toc lof lot lol nav snm vrb';

# Track extra extensions for recompilation
push @generated_exts, 'bcf', 'run.xml';
