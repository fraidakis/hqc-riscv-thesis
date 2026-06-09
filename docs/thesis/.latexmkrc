# =============================================================================
# latexmk configuration — LuaLaTeX
# =============================================================================

$pdf_mode = 4;  # lualatex

# Route all build artifacts to build/ subdirectory
$out_dir = 'build';
$aux_dir = 'build';

# Pre-create needed subdirectories in build/ for \include aux files
# build/figures-cache/ stores the tikz-externalized figure PDFs
# (prefix is 'figures-cache/' and -output-directory=build, so files land here)
use File::Path qw(make_path);
make_path("build/figures-cache", "build/chapters", "build/frontmatter", "build/appendices");

# Symlink figures-cache/ -> build/figures-cache/ at the thesis root.
# Needed because lualatex reads \includegraphics paths relative to CWD
# but -output-directory=build causes \openout to write there.
# The symlink makes both point to the same real directory.
symlink("build/figures-cache", "figures-cache")
    unless -e "figures-cache" || -l "figures-cache";

# Shell-escape for tikz/external, batchmode suppresses console output (logs go
# to build/main.log), -f forces latexmk to continue past non-fatal errors.
$lualatex = 'lualatex -shell-escape -interaction=batchmode -synctex=1 %O %S';
$force_mode = 1;  # continue even if lualatex returns non-zero

# Bibliography backend = biber. latexmk auto-detects the .bcf file and runs the
# $biber command (NOT $bibtex), so overriding $bibtex is dead code.
# Use the correct $biber command and force the bib step to always run.
$biber = 'biber %O %S';   # %S = build/main.bcf (out_dir-aware)
$bibtex_use = 2;           # always run biber when the document cites

# Clean extensions
# Note: build/figures-cache/ PDFs are NOT cleaned here (intentional cache).
# Use 'make extern-clean' to wipe only the figure cache.
$clean_ext = 'bbl bcf blg run.xml synctex.gz fdb_latexmk fls aux log out toc lof lot lol nav snm vrb dpth md5';

# Track extra extensions for recompilation
push @generated_exts, 'bcf', 'run.xml';
