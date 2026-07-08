#!/usr/bin/env bash
# compare-slides.sh — Build STATIC variants of one or more slides side-by-side
#                     for visual A/B comparison of the defense deck.
#
# Twin of docs/thesis/figures/compare-variants.sh, adapted for beamer frames.
#
# Convention (produced by @Skill — Slide Beautify & Variants):
#   base    : sections/<NN-name>.tex           (\input by main.tex)
#   variant : sections/<NN-name>_<suffix>.tex  (PARKED, not \input by main.tex)
#             e.g. 10-results_minimal.tex, 10-results_withfig.tex
#   Rule: a section file is a VARIANT iff its basename contains '_'
#         (section base names use hyphens only, never underscores).
#
# Variants are authored as STATIC, complete frames (no \pause). Comparison is
# therefore rendered in HANDOUT mode by default so every frame is exactly one
# page -> trivial 1:1 comparison and clean tab-splitting. Use --overlays to
# keep \pause reveals (then page<->frame mapping may break; falls back to
# opening the consolidated PDF instead of per-group tabs).
#
# What it does:
#   1. Discovers base + variant frame files for the requested section(s).
#   2. Builds ONE consolidated beamer PDF (real theme) with a footer tag per
#      frame naming the file it came from  ->  build/slide-compare/_slide-compare.pdf
#   3. (default) splits it to one-PDF-per-page and opens each figure group
#      (base + its variants) in its own Chrome window, variants as tabs.
#   4. (--context) additionally rebuilds the FULL deck with each variant swapped
#      into its narrative position, so you can judge flow (heavier, opt-in).
#
# Usage:
#   ./compare-slides.sh -s 10-results             # base + all its variants
#   ./compare-slides.sh -s results                # fuzzy match on section name
#   ./compare-slides.sh -s 10-results -f minimal  # only variants matching "minimal"
#   ./compare-slides.sh --all                     # every section that has variants
#   ./compare-slides.sh -s 10-results --context   # + full-deck-in-context view
#   ./compare-slides.sh -s 10-results --overlays  # keep \pause (>1 page/frame ok)
#   ./compare-slides.sh -s 10-results --no-open   # build only, don't launch browser
#   ./compare-slides.sh -s 10-results -j 8        # 8 parallel jobs (--context builds)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # .../presentation
SECTIONS_DIR="${SCRIPT_DIR}/sections"
BUILD_DIR="${SCRIPT_DIR}/build/slide-compare"

# --- Defaults ---------------------------------------------------------------
SECTION=""
FILTER=""
JOBS="${JOBS:-6}"
ALL_SECTIONS=false
CONTEXT=false
OVERLAYS=false
OPEN=true
ZOOM="${ZOOM:-292}"   # default Chrome PDF-viewer zoom (%) for every opened tab

usage() {
    cat <<EOF
Usage: $(basename "$0") [OPTIONS]

Build static slide variants and open them for side-by-side comparison.
Each base frame gets its own Chrome window with base + variants as tabs.

OPTIONS:
  -s, --section NAME   Section base name or fuzzy fragment
                       (e.g. 10-results, results, 06-scalar)
  -f, --filter PAT     Only include variants whose filename matches PAT (grep -i)
  -j, --jobs N         Parallel jobs for --context full-deck builds (default: $JOBS)
      --all            Every section that has at least one variant
      --context        Also build the full deck with each variant swapped in
      --overlays       Keep \pause reveals (default: handout-flatten to 1 page)
      --no-open        Build only; do not launch the browser
  -z, --zoom N         Chrome PDF-viewer zoom percent for opened tabs (default: $ZOOM)
  -h, --help           Show this help

EXAMPLES:
  $(basename "$0") -s 10-results
  $(basename "$0") -s results -f withfig
  $(basename "$0") --all
  $(basename "$0") -s 10-results --context
EOF
    exit "${1:-0}"
}

# --- Parse args -------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        -s|--section) SECTION="$2"; shift 2 ;;
        -f|--filter)  FILTER="$2";  shift 2 ;;
        -j|--jobs)    JOBS="$2";    shift 2 ;;
        --all)        ALL_SECTIONS=true; shift ;;
        --context)    CONTEXT=true; shift ;;
        --overlays)   OVERLAYS=true; shift ;;
        --no-open)    OPEN=false; shift ;;
        -z|--zoom)    ZOOM="$2"; shift 2 ;;
        -h|--help)    usage 0 ;;
        *)            echo "Error: unknown option '$1'"; usage 1 ;;
    esac
done

# --- Helpers ----------------------------------------------------------------
is_variant() { [[ "$(basename "$1" .tex)" == *_* ]]; }

# Turn a PDF path into a file:// URL carrying the default zoom so Chrome's
# built-in PDF viewer opens every tab at $ZOOM% instead of 100%.
to_url() {
    local p="$1"
    case "$p" in /*) : ;; *) p="$(cd "$(dirname "$p")" && pwd)/$(basename "$p")" ;; esac
    printf 'file://%s#zoom=%s' "$p" "$ZOOM"
}

# Resolve a section base file (no underscore) from exact name or fuzzy fragment.
resolve_section() {
    local input="${1%.tex}"
    if [[ -f "${SECTIONS_DIR}/${input}.tex" ]] && [[ "$input" != *_* ]]; then
        echo "$input"; return 0
    fi
    local matches=()
    for f in "$SECTIONS_DIR"/*.tex; do
        [[ -e "$f" ]] || continue
        local b; b="$(basename "$f" .tex)"
        [[ "$b" == *_* ]] && continue           # skip variants
        if echo "$b" | grep -qi "$input"; then matches+=("$b"); fi
    done
    if [[ ${#matches[@]} -eq 1 ]]; then
        echo "${matches[0]}"; return 0
    elif [[ ${#matches[@]} -gt 1 ]]; then
        echo "Error: ambiguous section '$input'. Matches:" >&2
        printf '  %s\n' "${matches[@]}" >&2
    fi
    return 1
}

# --- Determine base sections to process -------------------------------------
BASES=()
if [[ "$ALL_SECTIONS" == true ]]; then
    for f in "$SECTIONS_DIR"/*.tex; do
        [[ -e "$f" ]] || continue
        b="$(basename "$f" .tex)"
        [[ "$b" == *_* ]] && continue
        # keep only bases that actually have >=1 variant
        if compgen -G "${SECTIONS_DIR}/${b}_*.tex" > /dev/null; then BASES+=("$b"); fi
    done
    if [[ ${#BASES[@]} -eq 0 ]]; then echo "No sections with variants found."; exit 1; fi
elif [[ -n "$SECTION" ]]; then
    R=$(resolve_section "$SECTION") || { echo "Error: could not resolve section '$SECTION'."; \
        echo "Available:"; ls -1 "$SECTIONS_DIR"/*.tex 2>/dev/null | xargs -I{} basename {} .tex \
        | grep -v '_' | sed 's/^/  /'; exit 1; }
    BASES+=("$R")
else
    echo "Error: --section or --all is required."; usage 1
fi

# --- Collect ordered file groups (base first, then variants) ----------------
# GROUP_FILES[i] = newline-separated list of section basenames for group i
GROUP_LABEL=()
GROUP_FILES=()
for base in "${BASES[@]}"; do
    files=("$base")
    for vf in "${SECTIONS_DIR}/${base}"_*.tex; do
        [[ -e "$vf" ]] || continue
        vn="$(basename "$vf" .tex)"
        if [[ -n "$FILTER" ]]; then echo "$vn" | grep -qi "$FILTER" || continue; fi
        files+=("$vn")
    done
    if [[ ${#files[@]} -eq 1 ]]; then
        echo "Note: '$base' has no variants (or none matched --filter '$FILTER'); skipping."
        continue
    fi
    GROUP_LABEL+=("$base")
    GROUP_FILES+=("$(printf '%s\n' "${files[@]}")")
done

if [[ ${#GROUP_LABEL[@]} -eq 0 ]]; then echo "Nothing to build."; exit 1; fi

# --- Announce the build plan ------------------------------------------------
TOTAL_FRAMES=0
[[ "$OVERLAYS" == true ]] && MODE="overlays (\\pause kept)" || MODE="handout (1 page/frame)"
echo "=== Build plan ==="
echo "Groups: ${#GROUP_LABEL[@]}  |  Mode: ${MODE}  |  Zoom: ${ZOOM}%"
for g in "${!GROUP_LABEL[@]}"; do
    vcount=0
    while IFS= read -r sec; do [[ -n "$sec" ]] && vcount=$((vcount+1)); done <<< "${GROUP_FILES[$g]}"
    variants=$((vcount - 1))
    TOTAL_FRAMES=$((TOTAL_FRAMES + vcount))
    echo "  * ${GROUP_LABEL[$g]}: 1 base + ${variants} variant(s) = ${vcount} frames"
    while IFS= read -r sec; do
        [[ -n "$sec" ]] || continue
        [[ "$sec" == "${GROUP_LABEL[$g]}" ]] && continue
        echo "      - ${sec}"
    done <<< "${GROUP_FILES[$g]}"
done
echo "Total frames to build: ${TOTAL_FRAMES}"
[[ "$CONTEXT" == true ]] && echo "Plus full-deck context builds for every variant (--context)."
echo ""

mkdir -p "$BUILD_DIR"

# --- Build the consolidated comparison deck ---------------------------------
HANDOUT_OPT=",handout"
[[ "$OVERLAYS" == true ]] && HANDOUT_OPT=""

WRAPPER="${BUILD_DIR}/_slide-compare.tex"
{
    echo "% Auto-generated by compare-slides.sh — do not edit."
    echo "\\documentclass[aspectratio=169,10pt${HANDOUT_OPT}]{beamer}"
    echo "\\input{preamble}"
    echo "\\tikzexternaldisable"                       # render figures inline
    echo "\\renewcommand{\\tikzsetnextfilename}[1]{}"
    echo "\\newcommand{\\vlabel}{}"
    echo "\\setbeamertemplate{footline}{\\hfill\\tiny\\ttfamily\\vlabel\\hspace{1.5em}\\vspace{3pt}}"
    echo "\\begin{document}"
    for g in "${!GROUP_LABEL[@]}"; do
        while IFS= read -r sec; do
            [[ -n "$sec" ]] || continue
            echo "\\renewcommand{\\vlabel}{\\detokenize{${sec}}}"
            echo "\\input{sections/${sec}}"
        done <<< "${GROUP_FILES[$g]}"
    done
    echo "\\end{document}"
} > "$WRAPPER"

echo "=== Building consolidated comparison deck ==="
cd "$SCRIPT_DIR"
lualatex -shell-escape -interaction=batchmode \
    -output-directory="$BUILD_DIR" -jobname=_slide-compare \
    "$WRAPPER" > /dev/null 2>&1 || true

PDF="${BUILD_DIR}/_slide-compare.pdf"
if [[ ! -f "$PDF" ]]; then
    echo "✗ Build failed. See ${BUILD_DIR}/_slide-compare.log"; exit 1
fi
echo "✓ Consolidated deck: $PDF"

# --- Optional: full-deck-in-context builds ----------------------------------
if [[ "$CONTEXT" == true ]]; then
    echo ""
    echo "=== Building full-deck context views (--context) ==="
    CTX_JOBS="$BUILD_DIR/_ctx-jobs.txt"; : > "$CTX_JOBS"
    for g in "${!GROUP_LABEL[@]}"; do
        base="${GROUP_LABEL[$g]}"
        while IFS= read -r sec; do
            [[ -n "$sec" ]] || continue
            [[ "$sec" == "$base" ]] && continue     # skip the base itself
            echo "${base}|${sec}" >> "$CTX_JOBS"
        done <<< "${GROUP_FILES[$g]}"
    done
    build_ctx() {
        local base="${1%%|*}" variant="${1##*|}"
        local ctx="${BUILD_DIR}/_ctx-${variant}.tex"
        # Swap the base \input for the variant one, keep the rest of main.tex.
        sed "s|\\\\input{sections/${base}}|\\\\input{sections/${variant}}|" \
            "${SCRIPT_DIR}/main.tex" > "$ctx"
        ( cd "$SCRIPT_DIR" && latexmk -lualatex -shell-escape -interaction=batchmode \
            -output-directory="$BUILD_DIR" -jobname="_ctx-${variant}" "$ctx" > /dev/null 2>&1 ) || true
        if [[ -f "${BUILD_DIR}/_ctx-${variant}.pdf" ]]; then
            echo "✓ context: ${variant}"
        else
            echo "✗ context: ${variant} (see ${BUILD_DIR}/_ctx-${variant}.log)"
        fi
    }
    export -f build_ctx; export SCRIPT_DIR BUILD_DIR
    if [[ -s "$CTX_JOBS" ]]; then
        xargs -P "$JOBS" -I{} bash -c 'build_ctx "$@"' _ {} < "$CTX_JOBS" || true
    fi
fi

[[ "$OPEN" == false ]] && { echo ""; echo "Build only (--no-open). Artifacts in $BUILD_DIR"; exit 0; }

# --- Detect Chrome/Chromium -------------------------------------------------
CHROME=""
for c in google-chrome google-chrome-stable chromium-browser chromium chrome; do
    command -v "$c" >/dev/null 2>&1 && { CHROME="$c"; break; }
done
if [[ -z "$CHROME" ]]; then
    echo ""; echo "Chrome/Chromium not found. PDFs are in: $BUILD_DIR/"; exit 0
fi

# --- Overlays kept -> tab split unreliable; just open consolidated -----------
if [[ "$OVERLAYS" == true ]]; then
    echo ""; echo "--overlays set: opening consolidated deck (per-group tabs disabled)."
    "$CHROME" --new-window "$(to_url "$PDF")" >/dev/null 2>&1 &
    [[ "$CONTEXT" == true ]] && for p in "$BUILD_DIR"/_ctx-*.pdf; do [[ -e "$p" ]] && "$CHROME" --new-window "$(to_url "$p")" >/dev/null 2>&1 & sleep 0.3; done
    echo "Done."; exit 0
fi

# --- Split to one-PDF-per-page and open each group as tabs -------------------
EXPECTED=0
for g in "${!GROUP_LABEL[@]}"; do
    while IFS= read -r sec; do [[ -n "$sec" ]] && EXPECTED=$((EXPECTED+1)); done <<< "${GROUP_FILES[$g]}"
done

PAGES=0
if command -v pdfinfo >/dev/null 2>&1; then
    PAGES=$(pdfinfo "$PDF" 2>/dev/null | awk '/^Pages:/{print $2}')
fi

if ! command -v pdfseparate >/dev/null 2>&1 || [[ "${PAGES:-0}" -ne "$EXPECTED" ]]; then
    echo ""
    echo "Per-page split unavailable or page count (${PAGES:-?}) != expected ($EXPECTED)"
    echo "(a frame spilled to multiple pages). Opening the consolidated deck instead."
    "$CHROME" --new-window "$(to_url "$PDF")" >/dev/null 2>&1 &
    echo "Done."; exit 0
fi

rm -f "$BUILD_DIR"/page-*.pdf
pdfseparate "$PDF" "${BUILD_DIR}/page-%d.pdf"

echo ""
p=1
for g in "${!GROUP_LABEL[@]}"; do
    tabs=()
    while IFS= read -r sec; do
        [[ -n "$sec" ]] || continue
        tabs+=("$(to_url "${BUILD_DIR}/page-${p}.pdf")"); p=$((p+1))
    done <<< "${GROUP_FILES[$g]}"
    echo "Opening: ${GROUP_LABEL[$g]} (${#tabs[@]} tabs @ ${ZOOM}% zoom)"
    "$CHROME" --new-window "${tabs[@]}" >/dev/null 2>&1 &
    sleep 0.3
done

if [[ "$CONTEXT" == true ]]; then
    for pdf in "$BUILD_DIR"/_ctx-*.pdf; do
        [[ -e "$pdf" ]] || continue
        "$CHROME" --new-window "$(to_url "$pdf")" >/dev/null 2>&1 &
        sleep 0.3
    done
fi

echo ""
echo "Done. One window per slide group; consolidated deck at $PDF"
