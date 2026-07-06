# .latexmkrc — latexmk configuration for LuaLaTeX beamer deck
#
# NOTE: the engine itself (-lualatex -shell-escape) is passed via the
# Makefile's $(FLAGS) on every invocation, together with an EXPLICIT
# $(MAIN).tex target. @default_files below is only a safety net for a
# bare `latexmk` call with no arguments -- without it, latexmk's own
# default glob ('*.tex') would also try to compile preamble.tex and
# backup.tex as standalone root documents (no \documentclass) and fail
# with "Missing \begin{document}" / "\usepackage before \documentclass".
@default_files = ('main.tex');

$out_dir = 'build';   # all build artifacts isolated here
$aux_dir = 'build';   # keeps the source tree clean
$silent  = 0;

# Ensure build directory exists
system('mkdir -p build');
