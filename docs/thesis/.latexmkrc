# =============================================================================
# latexmk configuration — LuaLaTeX
# =============================================================================

$pdf_mode = 4;  # lualatex

# Route all build artifacts to build/ subdirectory
$out_dir = 'build';
$aux_dir = 'build';

# Pre-create needed subdirectories in build/ for \include aux files and
# the tikz externalization cache.
use File::Path qw(make_path);
make_path("build/figures-cache", "build/chapters", "build/frontmatter", "build/appendices");

# Symlink figures-cache/ -> build/figures-cache/ at the thesis root.
# Needed because tikz/external uses prefix=figures-cache/ (relative to CWD),
# while -output-directory=build causes lualatex to write files to build/.
# The symlink makes both resolve to the same real directory.
# It is .gitignored and recreated automatically on every latexmk run.
symlink("build/figures-cache", "figures-cache")
    unless -e "figures-cache" || -l "figures-cache";

# Shell-escape for tikz/external, batchmode suppresses console output (logs go
# to build/main.log), -f forces latexmk to continue past non-fatal errors.
$lualatex = 'lualatex -shell-escape -interaction=batchmode -synctex=1 %O %S';
$f = 1;  # latexmk -f: continue even if lualatex returns non-zero

# Bibliography backend = biber. latexmk auto-detects the .bcf file and runs the
# $biber command (NOT $bibtex), so the previous `$bibtex = 'biber ...'` override
# was dead code; worse, its `--input-directory .` pointed biber at the project
# root while the .bcf lives under $out_dir=build/, so if it had ever been used it
# would fail to find build/main.bcf and silently leave main.bbl unbuilt.
# Use the correct $biber command and force the bib step to always run, so the
# bibliography/cross-reference cycle (lualatex -> biber -> lualatex x2) can never
# be skipped (which is what makes every \cite / \cref render as "??").
$biber = 'biber %O %S';   # %S = build/main.bcf (out_dir-aware)
$bibtex_use = 2;          # always run biber when the document cites

# Clean extensions
# Note: build/figures-cache/ PDFs are NOT cleaned here (intentional cache).
# Use 'make extern-clean' to wipe only the figure cache.
$clean_ext = 'bbl bcf blg run.xml synctex.gz fdb_latexmk fls aux log out toc lof lot lol nav snm vrb dpth md5';

# Track extra extensions for recompilation
push @generated_exts, 'bcf', 'run.xml';
